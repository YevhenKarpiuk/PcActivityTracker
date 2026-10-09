#include "ui/ReportView.h"
#include "reports/ReportEngine.h"
#include "reports/ReportExporter.h"
#include "core/TimeUtil.h"

#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace pcat {
namespace {
std::string pathUtf8(const std::filesystem::path& path){const auto value=path.u8string();return std::string(reinterpret_cast<const char*>(value.data()),value.size());}
bool hasGroup(const ReportDefinition& d,GroupField g){return std::find(d.groups.begin(),d.groups.end(),g)!=d.groups.end();}
bool hasMetric(const ReportDefinition& d,Metric m){return std::find(d.metrics.begin(),d.metrics.end(),m)!=d.metrics.end();}
void toggleGroup(ReportDefinition& d,GroupField g,bool on){auto it=std::find(d.groups.begin(),d.groups.end(),g);if(on&&it==d.groups.end())d.groups.push_back(g);if(!on&&it!=d.groups.end())d.groups.erase(it);}
void toggleMetric(ReportDefinition& d,Metric m,bool on){auto it=std::find(d.metrics.begin(),d.metrics.end(),m);if(on&&it==d.metrics.end())d.metrics.push_back(m);if(!on&&it!=d.metrics.end()&&d.metrics.size()>1)d.metrics.erase(it);}
template<class T>void copyText(T& dst,const std::string& s){std::snprintf(dst.data(),dst.size(),"%s",s.c_str());}
std::string dateText(std::chrono::system_clock::time_point tp){auto tm=timeutil::localTm(tp);char b[16]{};std::strftime(b,sizeof(b),"%Y-%m-%d",&tm);return b;}
bool parseDate(const char* s,std::tm& tm){if(!s||std::strlen(s)!=10||s[4]!='-'||s[7]!='-')return false;auto digit=[](char c){return c>='0'&&c<='9';};for(int i:{0,1,2,3,5,6,8,9})if(!digit(s[i]))return false;const int y=(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+(s[3]-'0');const int m=(s[5]-'0')*10+(s[6]-'0');const int d=(s[8]-'0')*10+(s[9]-'0');const std::chrono::year_month_day ymd{std::chrono::year{y},std::chrono::month{static_cast<unsigned>(m)},std::chrono::day{static_cast<unsigned>(d)}};if(!ymd.ok())return false;tm={};tm.tm_year=y-1900;tm.tm_mon=m-1;tm.tm_mday=d;tm.tm_isdst=-1;return true;}
double metricNumber(const ReportValues& v,Metric m){switch(m){case Metric::TotalSeconds:return v.totalSeconds/3600.0;case Metric::ActiveSeconds:return v.activeSeconds/3600.0;case Metric::IdleSeconds:return v.idleSeconds/3600.0;case Metric::RecordCount:return static_cast<double>(v.recordCount);case Metric::AverageSeconds:return v.averageSeconds/60.0;case Metric::MinSeconds:return v.minSeconds/60.0;case Metric::MaxSeconds:return v.maxSeconds/60.0;case Metric::SharePercent:return v.sharePercent;}return 0;}
const char* metricUnit(Metric m){switch(m){case Metric::TotalSeconds:case Metric::ActiveSeconds:case Metric::IdleSeconds:return "часы";case Metric::AverageSeconds:case Metric::MinSeconds:case Metric::MaxSeconds:return "мин";case Metric::RecordCount:return "шт";case Metric::SharePercent:return "%";}return "";}
void drawMetric(const ReportValues& v,Metric m){switch(m){case Metric::TotalSeconds:ImGui::TextUnformatted(timeutil::formatDuration(v.totalSeconds).c_str());break;case Metric::ActiveSeconds:ImGui::TextUnformatted(timeutil::formatDuration(v.activeSeconds).c_str());break;case Metric::IdleSeconds:ImGui::TextUnformatted(timeutil::formatDuration(v.idleSeconds).c_str());break;case Metric::RecordCount:ImGui::Text("%lld",v.recordCount);break;case Metric::AverageSeconds:ImGui::TextUnformatted(timeutil::formatDuration(static_cast<long long>(std::llround(v.averageSeconds))).c_str());break;case Metric::MinSeconds:ImGui::TextUnformatted(timeutil::formatDuration(v.minSeconds).c_str());break;case Metric::MaxSeconds:ImGui::TextUnformatted(timeutil::formatDuration(v.maxSeconds).c_str());break;case Metric::SharePercent:ImGui::Text("%.2f",v.sharePercent);break;}}
std::string rowLabel(const ReportRow& row){if(row.kind==RowKind::GrandTotal)return "ИТОГО";std::string out;for(std::size_t i=0;i<row.keys.size();++i){if(i)out+=" / ";out+=row.keys[i];}return out.empty()?"—":out;}
std::string displayKey(const ReportRow& row,std::size_t column){if(row.kind==RowKind::GrandTotal)return column==0?"ИТОГО":"";if(column<row.keys.size())return row.keys[column];if(row.kind==RowKind::Subtotal&&column==row.keys.size())return "Итого";return {};}
void drawPie(const std::vector<double>& values,const std::vector<std::string>& labels){
    if(values.empty())return;
    double total=0;
    for(auto v:values)if(v>0)total+=v;
    if(total<=0)return;
    const ImVec2 area=ImGui::GetContentRegionAvail();const float height=245.0f;const float radius=std::min(90.0f,height*0.38f);const ImVec2 origin=ImGui::GetCursorScreenPos();const ImVec2 center(origin.x+radius+12.0f,origin.y+height*0.5f);auto* draw=ImGui::GetWindowDrawList();double angle=-3.14159265358979323846/2.0;
    for(std::size_t i=0;i<values.size();++i){if(values[i]<=0)continue;const double next=angle+2.0*3.14159265358979323846*(values[i]/total);const ImU32 color=ImColor::HSV(static_cast<float>((i*0.61803398875)-std::floor(i*0.61803398875)),0.60f,0.90f);draw->PathLineTo(center);draw->PathArcTo(center,radius,static_cast<float>(angle),static_cast<float>(next),24);draw->PathFillConvex(color);angle=next;}
    ImGui::Dummy(ImVec2(radius*2.0f+24.0f,height));ImGui::SameLine();ImGui::BeginGroup();for(std::size_t i=0;i<labels.size()&&i<12;++i){const ImU32 color=ImColor::HSV(static_cast<float>((i*0.61803398875)-std::floor(i*0.61803398875)),0.60f,0.90f);ImGui::ColorButton(("##pie"+std::to_string(i)).c_str(),ImGui::ColorConvertU32ToFloat4(color),ImGuiColorEditFlags_NoTooltip,ImVec2(12,12));ImGui::SameLine();ImGui::Text("%s: %.2f",labels[i].c_str(),values[i]);}if(labels.size()>12)ImGui::Text("... ещё %zu",labels.size()-12);ImGui::EndGroup();(void)area;
}
}

void ReportView::initialize(){auto now=std::chrono::system_clock::now();auto week=now-std::chrono::hours(24*6);copyText(startDate_,dateText(week));copyText(endDate_,dateText(now));def_.groups={GroupField::Day,GroupField::Process};def_.metrics={Metric::TotalSeconds,Metric::ActiveSeconds,Metric::IdleSeconds,Metric::RecordCount,Metric::SharePercent};def_.showSubtotals=true;def_.showGrandTotal=true;initialized_=true;}

void ReportView::rebuild(ActivityMonitor& monitor){
    std::tm startTm{},endTm{};if(!parseDate(startDate_.data(),startTm)||!parseDate(endDate_.data(),endTm)){status_="Некорректная дата. Формат: YYYY-MM-DD";return;}def_.start=timeutil::fromLocalTm(startTm);endTm.tm_mday+=1;def_.end=timeutil::fromLocalTm(endTm);if(def_.end<=def_.start){status_="Дата 'По' должна быть не раньше даты 'С'";return;}
    def_.filter.processContains=processFilter_.data();def_.filter.categoryContains=categoryFilter_.data();def_.filter.domainContains=domainFilter_.data();def_.filter.titleContains=titleFilter_.data();
    try{auto snapshot=monitor.readSnapshot(def_.start,def_.end);reportRecords_=std::move(snapshot.records);if(snapshot.current)reportRecords_.push_back(std::move(*snapshot.current));result_=buildReport(reportRecords_,def_);status_="Сформировано: "+std::to_string(result_.grandTotal.recordCount)+" периодов";}catch(const std::exception&e){status_=std::string("Ошибка отчёта: ")+e.what();}
}

void ReportView::draw(ActivityMonitor& monitor){
    if(!initialized_){initialize();rebuild(monitor);}
    ImGui::TextUnformatted("Период (включительно)");ImGui::SetNextItemWidth(120);ImGui::InputText("С##date",startDate_.data(),startDate_.size());ImGui::SameLine();ImGui::SetNextItemWidth(120);ImGui::InputText("По##date",endDate_.data(),endDate_.size());ImGui::SameLine();ImGui::SetNextItemWidth(90);ImGui::InputInt("Top N",&def_.topN);if(def_.topN<0)def_.topN=0;ImGui::SameLine();ImGui::Checkbox("Включать простой",&def_.filter.includeIdle);
    ImGui::SameLine();ImGui::SetNextItemWidth(170);if(ImGui::BeginCombo("Сортировка",metricName(def_.sortMetric).c_str())){for(auto m:metricOrder_){const bool selected=m==def_.sortMetric;if(ImGui::Selectable(metricName(m).c_str(),selected))def_.sortMetric=m;if(selected)ImGui::SetItemDefaultFocus();}ImGui::EndCombo();}ImGui::SameLine();ImGui::Checkbox("По убыванию",&def_.sortDescending);
    ImGui::TextUnformatted("Фильтры");ImGui::SetNextItemWidth(180);ImGui::InputText("Программа##filter",processFilter_.data(),processFilter_.size());ImGui::SameLine();ImGui::SetNextItemWidth(180);ImGui::InputText("Категория##filter",categoryFilter_.data(),categoryFilter_.size());ImGui::SameLine();ImGui::SetNextItemWidth(180);ImGui::InputText("Домен##filter",domainFilter_.data(),domainFilter_.size());ImGui::SameLine();ImGui::SetNextItemWidth(240);ImGui::InputText("Окно##filter",titleFilter_.data(),titleFilter_.size());
    ImGui::SeparatorText("Группировки");for(auto g:groupOrder_){bool v=hasGroup(def_,g);if(ImGui::Checkbox(groupFieldName(g).c_str(),&v))toggleGroup(def_,g,v);ImGui::SameLine();}ImGui::NewLine();
    ImGui::SeparatorText("Показатели");for(auto m:metricOrder_){bool v=hasMetric(def_,m);const auto id=metricName(m)+"##metric";if(ImGui::Checkbox(id.c_str(),&v))toggleMetric(def_,m,v);ImGui::SameLine();}ImGui::NewLine();
    ImGui::Checkbox("Промежуточные итоги",&def_.showSubtotals);ImGui::SameLine();ImGui::Checkbox("Общий ИТОГО",&def_.showGrandTotal);ImGui::SameLine();if(ImGui::Button("Сформировать отчёт"))rebuild(monitor);ImGui::SameLine();if(ImGui::Button("Экспорт CSV")){try{const auto file=exportDirectory_/"report.csv";exportReportCsv(result_,file);status_="Сохранено: "+pathUtf8(file);}catch(const std::exception&e){status_=e.what();}}ImGui::SameLine();if(ImGui::Button("Экспорт JSON")){try{const auto file=exportDirectory_/"report.json";exportReportJson(result_,file);status_="Сохранено: "+pathUtf8(file);}catch(const std::exception&e){status_=e.what();}}
    if(!status_.empty())ImGui::TextUnformatted(status_.c_str());
    ImGui::Text("Общий итог: %s | активно %s | простой %s | периодов %lld",timeutil::formatDuration(result_.grandTotal.totalSeconds).c_str(),timeutil::formatDuration(result_.grandTotal.activeSeconds).c_str(),timeutil::formatDuration(result_.grandTotal.idleSeconds).c_str(),result_.grandTotal.recordCount);

    // The controls above edit the next report definition. Keep the currently displayed table bound
    // to the definition that actually produced result_, otherwise toggling a grouping before clicking
    // "Сформировать отчёт" would immediately misalign existing keys and columns.
    const auto& shownDef=result_.definition;
    const int columns=static_cast<int>(shownDef.groups.size()+shownDef.metrics.size());
    if(columns>0&&ImGui::BeginTable("report",columns,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,ImVec2(0,330))){
        for(auto g:shownDef.groups)ImGui::TableSetupColumn(groupFieldName(g).c_str());
        for(auto m:shownDef.metrics)ImGui::TableSetupColumn(metricName(m).c_str());
        ImGui::TableHeadersRow();
        for(const auto& row:result_.rows){ImGui::TableNextRow();if(row.kind==RowKind::GrandTotal)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,IM_COL32(72,84,100,255));else if(row.kind==RowKind::Subtotal)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,IM_COL32(52,62,74,255));int column=0;for(std::size_t i=0;i<shownDef.groups.size();++i){ImGui::TableSetColumnIndex(column++);const auto text=displayKey(row,i);if(row.kind!=RowKind::Detail&&(!text.empty())){ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,1,1,1));ImGui::TextUnformatted(text.c_str());ImGui::PopStyleColor();}else ImGui::TextUnformatted(text.c_str());}for(auto m:shownDef.metrics){ImGui::TableSetColumnIndex(column++);drawMetric(row.values,m);}}
        ImGui::EndTable();
    }

    if(ImGui::BeginCombo("Показатель графика",metricName(chartMetric_).c_str())){for(auto m:metricOrder_){const bool selected=m==chartMetric_;if(ImGui::Selectable(metricName(m).c_str(),selected))chartMetric_=m;if(selected)ImGui::SetItemDefaultFocus();}ImGui::EndCombo();}ImGui::SameLine();
    const char* chartNames[]={"Столбцы","Линия","Круговая"};int chartIndex=static_cast<int>(chartType_);ImGui::SetNextItemWidth(130);if(ImGui::Combo("Тип графика",&chartIndex,chartNames,3))chartType_=static_cast<ChartType>(chartIndex);
    std::vector<double> ys,xs;std::vector<std::string> labels;for(const auto& row:result_.rows)if(row.kind==RowKind::Detail){xs.push_back(static_cast<double>(xs.size()));ys.push_back(metricNumber(row.values,chartMetric_));labels.push_back(rowLabel(row));if(ys.size()>=60)break;}
    if(!ys.empty()){
        if(chartType_==ChartType::Pie)drawPie(ys,labels);
        else if(ImPlot::BeginPlot("График отчёта",ImVec2(-1,245))){std::vector<const char*> labelPtrs;labelPtrs.reserve(labels.size());for(auto& label:labels)labelPtrs.push_back(label.c_str());ImPlot::SetupAxes("Группа",metricUnit(chartMetric_),ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);if(labels.size()<=30)ImPlot::SetupAxisTicks(ImAxis_X1,xs.data(),static_cast<int>(xs.size()),labelPtrs.data(),false);if(chartType_==ChartType::Bars)ImPlot::PlotBars(metricName(chartMetric_).c_str(),xs.data(),ys.data(),static_cast<int>(ys.size()),0.7);else ImPlot::PlotLine(metricName(chartMetric_).c_str(),xs.data(),ys.data(),static_cast<int>(ys.size()));ImPlot::EndPlot();}
    }
}
}
