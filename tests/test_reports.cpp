#include "reports/ReportEngine.h"
#include "core/TimeUtil.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace pcat;
static ActivityRecord rec(long long id,const char*s,const char*e,bool idle,const char*proc,const char*cat){ActivityRecord r;r.id=id;r.startTime=timeutil::parseIso8601(s);r.endTime=timeutil::parseIso8601(e);r.durationSeconds=std::chrono::duration_cast<std::chrono::seconds>(r.endTime-r.startTime).count();r.isIdle=idle;r.processName=proc;r.category=cat;return r;}
int main(){
 std::vector<ActivityRecord> rs{
  rec(1,"2026-08-24T08:30:00+03:00","2026-08-24T09:30:00+03:00",false,"code.exe","Разработка"),
  rec(2,"2026-08-24T09:30:00+03:00","2026-08-24T10:00:00+03:00",true,"code.exe","Разработка"),
  rec(3,"2026-08-24T10:00:00+03:00","2026-08-24T11:00:00+03:00",false,"chrome.exe","Браузер")
 };
 ReportDefinition d;d.start=timeutil::parseIso8601("2026-08-24T00:00:00+03:00");d.end=timeutil::parseIso8601("2026-08-25T00:00:00+03:00");d.groups={GroupField::Category,GroupField::Hour};d.showSubtotals=true;d.showGrandTotal=true;
 auto r=buildReport(rs,d);assert(r.grandTotal.totalSeconds==9000);assert(r.grandTotal.activeSeconds==7200);assert(r.grandTotal.idleSeconds==1800);assert(r.grandTotal.recordCount==3);assert(std::abs(r.grandTotal.sharePercent-100.0)<0.001);
 bool subtotal=false,grand=false;long long detailSum=0;for(auto&x:r.rows){if(x.kind==RowKind::Detail)detailSum+=x.values.totalSeconds;if(x.kind==RowKind::Subtotal)subtotal=true;if(x.kind==RowKind::GrandTotal)grand=true;}assert(subtotal);assert(grand);assert(detailSum==9000);

 // A record split by hour is still one original period in the grand total. Each hour gets its clipped contribution.
 ReportDefinition h=d;h.groups={GroupField::Hour};h.showSubtotals=false;auto hr=buildReport({rs[0]},h);int details=0;for(auto&x:hr.rows)if(x.kind==RowKind::Detail){++details;assert(x.values.totalSeconds==1800);assert(x.values.recordCount==1);assert(std::llround(x.values.averageSeconds)==1800);}assert(details==2);assert(hr.grandTotal.recordCount==1);assert(std::llround(hr.grandTotal.averageSeconds)==3600);assert(hr.grandTotal.minSeconds==3600&&hr.grandTotal.maxSeconds==3600);

 // Fractional timestamps split at an hour boundary must not lose seconds due to per-piece flooring.
 auto fractional=rec(20,"2026-08-24T08:59:59.8000000+03:00","2026-08-24T09:00:01.8000000+03:00",false,"code","Разработка");
 ReportDefinition fractionalDef=d;fractionalDef.groups={GroupField::Hour};fractionalDef.showSubtotals=false;auto fr=buildReport({fractional},fractionalDef);long long fractionalDetail=0;for(const auto& row:fr.rows)if(row.kind==RowKind::Detail)fractionalDetail+=row.values.totalSeconds;assert(fr.grandTotal.totalSeconds==2);assert(fractionalDetail==2);

 // UTF-8 case-insensitive filters must work for Russian/Ukrainian category text.
 ReportDefinition unicode=d;unicode.groups={GroupField::Category};unicode.filter.categoryContains="разРАБОТКа";auto ur=buildReport(rs,unicode);assert(ur.grandTotal.totalSeconds==5400);assert(ur.grandTotal.recordCount==2);

 // Top N is applied per parent group and omitted children become "Остальные", preserving subtotals/grand total.
 std::vector<ActivityRecord> topRows{
  rec(10,"2026-08-24T08:00:00+03:00","2026-08-24T09:00:00+03:00",false,"a","X"),
  rec(11,"2026-08-24T09:00:00+03:00","2026-08-24T09:30:00+03:00",false,"b","X"),
  rec(12,"2026-08-24T09:30:00+03:00","2026-08-24T09:40:00+03:00",false,"c","X")
 };
 ReportDefinition top=d;top.groups={GroupField::Category,GroupField::Process};top.topN=1;auto tr=buildReport(topRows,top);assert(tr.grandTotal.totalSeconds==6000);long long visible=0;bool other=false;for(const auto&row:tr.rows)if(row.kind==RowKind::Detail){visible+=row.values.totalSeconds;if(!row.keys.empty()&&row.keys.back()=="Остальные")other=true;}assert(visible==6000);assert(other);

 // A totals-only report must contain exactly one grand-total row (when enabled).
 ReportDefinition totals=d;totals.groups.clear();totals.showGrandTotal=true;auto only=buildReport(rs,totals);assert(only.rows.size()==1);assert(only.rows[0].kind==RowKind::GrandTotal);assert(only.rows[0].values.totalSeconds==9000);

 // Sort direction is part of report semantics and must affect leaf order without breaking totals.
 ReportDefinition sorted=d;sorted.groups={GroupField::Process};sorted.showSubtotals=false;sorted.sortMetric=Metric::TotalSeconds;sorted.sortDescending=false;auto sr=buildReport(rs,sorted);std::vector<long long> orderedTotals;for(const auto& row:sr.rows)if(row.kind==RowKind::Detail)orderedTotals.push_back(row.values.totalSeconds);assert(orderedTotals.size()==2);assert(orderedTotals[0]<=orderedTotals[1]);assert(sr.grandTotal.totalSeconds==9000);
 std::cout<<"report tests passed\n";
}
