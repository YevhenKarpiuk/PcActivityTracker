#include "reports/ReportEngine.h"
#include "core/TimeUtil.h"
#include <cassert>
#include <ctime>
#include <cmath>
#include <iostream>
using namespace pcat;
// Группировки День/Час/День недели считаются в локальной зоне машины. Поэтому фикстуры
// задаются локальным временем, а не фиксированным смещением: иначе тест ломается там, где
// смещение не кратно часу (например +05:30) — граница часа попадает не туда, где её ждут.
static std::chrono::system_clock::time_point local(int year,int month,int day,int hour,int minute,int second=0){
    std::tm tm{};tm.tm_year=year-1900;tm.tm_mon=month-1;tm.tm_mday=day;tm.tm_hour=hour;tm.tm_min=minute;tm.tm_sec=second;tm.tm_isdst=-1;
    return timeutil::fromLocalTm(tm);
}
static ActivityRecord recAt(long long id,std::chrono::system_clock::time_point s,std::chrono::system_clock::time_point e,bool idle,const char*proc,const char*cat){ActivityRecord r;r.id=id;r.startTime=s;r.endTime=e;r.durationSeconds=std::chrono::duration_cast<std::chrono::seconds>(e-s).count();r.isIdle=idle;r.processName=proc;r.category=cat;return r;}
int main(){
 std::vector<ActivityRecord> rs{
  recAt(1,local(2026,8,24,8,30),local(2026,8,24,9,30),false,"code.exe","Разработка"),
  recAt(2,local(2026,8,24,9,30),local(2026,8,24,10,0),true,"code.exe","Разработка"),
  recAt(3,local(2026,8,24,10,0),local(2026,8,24,11,0),false,"chrome.exe","Браузер")
 };
 ReportDefinition d;d.start=local(2026,8,24,0,0);d.end=local(2026,8,25,0,0);d.groups={GroupField::Category,GroupField::Hour};d.showSubtotals=true;d.showGrandTotal=true;
 auto r=buildReport(rs,d);assert(r.grandTotal.totalSeconds==9000);assert(r.grandTotal.activeSeconds==7200);assert(r.grandTotal.idleSeconds==1800);assert(r.grandTotal.recordCount==3);assert(std::abs(r.grandTotal.sharePercent-100.0)<0.001);
 bool subtotal=false,grand=false;long long detailSum=0;for(auto&x:r.rows){if(x.kind==RowKind::Detail)detailSum+=x.values.totalSeconds;if(x.kind==RowKind::Subtotal)subtotal=true;if(x.kind==RowKind::GrandTotal)grand=true;}assert(subtotal);assert(grand);assert(detailSum==9000);

 // A record split by hour is still one original period in the grand total. Each hour gets its clipped contribution.
 ReportDefinition h=d;h.groups={GroupField::Hour};h.showSubtotals=false;auto hr=buildReport({rs[0]},h);int details=0;for(auto&x:hr.rows)if(x.kind==RowKind::Detail){++details;assert(x.values.totalSeconds==1800);assert(x.values.recordCount==1);assert(std::llround(x.values.averageSeconds)==1800);}assert(details==2);assert(hr.grandTotal.recordCount==1);assert(std::llround(hr.grandTotal.averageSeconds)==3600);assert(hr.grandTotal.minSeconds==3600&&hr.grandTotal.maxSeconds==3600);

 // Fractional timestamps split at an hour boundary must not lose seconds due to per-piece flooring.
 auto fractional=recAt(20,local(2026,8,24,8,59,59)+std::chrono::milliseconds(800),local(2026,8,24,9,0,1)+std::chrono::milliseconds(800),false,"code","Разработка");
 ReportDefinition fractionalDef=d;fractionalDef.groups={GroupField::Hour};fractionalDef.showSubtotals=false;auto fr=buildReport({fractional},fractionalDef);long long fractionalDetail=0;for(const auto& row:fr.rows)if(row.kind==RowKind::Detail)fractionalDetail+=row.values.totalSeconds;assert(fr.grandTotal.totalSeconds==2);assert(fractionalDetail==2);

 // UTF-8 case-insensitive filters must work for Russian/Ukrainian category text.
 ReportDefinition unicode=d;unicode.groups={GroupField::Category};unicode.filter.categoryContains="разРАБОТКа";auto ur=buildReport(rs,unicode);assert(ur.grandTotal.totalSeconds==5400);assert(ur.grandTotal.recordCount==2);

 // Top N is applied per parent group and omitted children become "Остальные", preserving subtotals/grand total.
 std::vector<ActivityRecord> topRows{
  recAt(10,local(2026,8,24,8,0),local(2026,8,24,9,0),false,"a","X"),
  recAt(11,local(2026,8,24,9,0),local(2026,8,24,9,30),false,"b","X"),
  recAt(12,local(2026,8,24,9,30),local(2026,8,24,9,40),false,"c","X")
 };
 ReportDefinition top=d;top.groups={GroupField::Category,GroupField::Process};top.topN=1;auto tr=buildReport(topRows,top);assert(tr.grandTotal.totalSeconds==6000);long long visible=0;bool other=false;for(const auto&row:tr.rows)if(row.kind==RowKind::Detail){visible+=row.values.totalSeconds;if(!row.keys.empty()&&row.keys.back()=="Остальные (Top N)")other=true;}assert(visible==6000);assert(other);

 // A totals-only report must contain exactly one grand-total row (when enabled).
 ReportDefinition totals=d;totals.groups.clear();totals.showGrandTotal=true;auto only=buildReport(rs,totals);assert(only.rows.size()==1);assert(only.rows[0].kind==RowKind::GrandTotal);assert(only.rows[0].values.totalSeconds==9000);

 // Sort direction is part of report semantics and must affect leaf order without breaking totals.
 ReportDefinition sorted=d;sorted.groups={GroupField::Process};sorted.showSubtotals=false;sorted.sortMetric=Metric::TotalSeconds;sorted.sortDescending=false;auto sr=buildReport(rs,sorted);std::vector<long long> orderedTotals;for(const auto& row:sr.rows)if(row.kind==RowKind::Detail)orderedTotals.push_back(row.values.totalSeconds);assert(orderedTotals.size()==2);assert(orderedTotals[0]<=orderedTotals[1]);assert(sr.grandTotal.totalSeconds==9000);
 // Multi-level ordering must follow the chosen metric at every level, not just the deepest one.
 // Previously the upper levels fell back to alphabetical order and "Alpha" always came first.
 {
  std::vector<ActivityRecord> levels{
   recAt(30,local(2026,8,24,8,0),local(2026,8,24,8,10),false,"p1","Alpha"),
   recAt(31,local(2026,8,24,9,0),local(2026,8,24,10,0),false,"p2","Beta")
  };
  ReportDefinition multi=d;multi.groups={GroupField::Category,GroupField::Process};multi.showSubtotals=false;
  multi.sortMetric=Metric::TotalSeconds;multi.sortDescending=true;
  auto mr=buildReport(levels,multi);
  std::vector<std::string> order;for(const auto& row:mr.rows)if(row.kind==RowKind::Detail)order.push_back(row.keys.front());
  assert(order.size()==2);
  assert(order[0]=="Beta");
  assert(order[1]=="Alpha");
  multi.sortDescending=false;
  auto ar=buildReport(levels,multi);
  std::vector<std::string> ascending;for(const auto& row:ar.rows)if(row.kind==RowKind::Detail)ascending.push_back(row.keys.front());
  assert(ascending.size()==2&&ascending[0]=="Alpha"&&ascending[1]=="Beta");
 }

 // A real group literally named "Остальные" must not be overwritten by the Top N remainder bucket.
 {
  std::vector<ActivityRecord> clash{
   recAt(40,local(2026,8,24,8,0),local(2026,8,24,9,0),false,"Остальные","X"),
   recAt(41,local(2026,8,24,9,0),local(2026,8,24,9,10),false,"b","X"),
   recAt(42,local(2026,8,24,9,10),local(2026,8,24,9,15),false,"c","X")
  };
  ReportDefinition top=d;top.groups={GroupField::Category,GroupField::Process};top.topN=1;top.showSubtotals=false;
  auto cr=buildReport(clash,top);
  assert(cr.grandTotal.totalSeconds==4500);
  long long visible=0;bool renamed=false;
  for(const auto& row:cr.rows)if(row.kind==RowKind::Detail){visible+=row.values.totalSeconds;if(row.keys.back()=="Остальные (Top N)")renamed=true;}
  assert(visible==4500);
  assert(renamed);
 }

 // Both the base remainder label and its numbered variants may be real groups.
 {
  std::vector<ActivityRecord> clash{
   recAt(50,local(2026,8,24,8,0),local(2026,8,24,8,10),false,"Остальные (Top N)","X"),
   recAt(51,local(2026,8,24,9,0),local(2026,8,24,9,5),false,"Остальные (Top N) #2","X"),
   recAt(52,local(2026,8,24,10,0),local(2026,8,24,10,1),false,"third","X")
  };
  auto collision=d;collision.groups={GroupField::Process};collision.topN=1;collision.showGrandTotal=false;
  const auto report=buildReport(clash,collision);
  assert(report.rows.size()==2);
  assert(report.rows[0].keys[0]=="Остальные (Top N)"&&report.rows[0].values.totalSeconds==600);
  assert(report.rows[1].keys[0]=="Остальные (Top N) #3"&&report.rows[1].values.totalSeconds==360);
  assert(report.grandTotal.totalSeconds==960&&report.grandTotal.recordCount==3);
 }

 // The Today view uses ReportEngine semantics: clip midnight and choose the top active app,
 // rather than ranking by total time (which could select an idle-only process).
 {
  const auto midnight=local(2026,8,24,0,0);
  const auto summary=buildTodaySummary({
   recAt(60,local(2026,8,23,23,50),local(2026,8,24,0,10),false,"editor","X"),
   recAt(61,local(2026,8,24,0,10),local(2026,8,24,1,0),true,"browser","X")
  },midnight,local(2026,8,24,1,0));
  assert(summary.values.activeSeconds==600&&summary.values.idleSeconds==3000);
  assert(summary.topProcess=="editor"&&summary.topActiveSeconds==600);
 }

 std::cout<<"report tests passed\n";
}
