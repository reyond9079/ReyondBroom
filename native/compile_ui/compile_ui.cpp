// SPDX-License-Identifier: GPL-3.0-or-later
// Compile layout and task forms adapted from TrenchBroom v2026.2 (Kristian Duske
// and contributors). Shared TB Qt controls remain unmodified in third_party.
#include "api.h"
#include "owner.h"
#include "third_party/json.hpp"
#include "ui/BitmapButton.h"
#include "ui/BorderLine.h"
#include "ui/ControlListBox.h"
#include "ui/DialogButtonLayout.h"
#include "ui/ElidedLabel.h"
#include "ui/EmptyWidget.h"
#include "ui/FixedWidthFont.h"
#include "ui/MiniToolBarLayout.h"
#include "ui/MultiCompletionLineEdit.h"
#include "ui/QStyleUtils.h"
#include "ui/TitledPanel.h"
#include "ui/Splitter.h"
#include "ui/ViewConstants.h"
#include <QtWidgets>
#include <filesystem>
#include <functional>
#include <memory>
using J=nlohmann::json;
using namespace tb::ui;
extern std::filesystem::path reyondCompileResources;
namespace {
ReyondCompileRequest host=nullptr;
std::unique_ptr<QApplication> ownedApplication;
QString qs(const std::string& s){return QString::fromUtf8(s.data(),int(s.size()));}
QString qs(const std::u8string& s){return QString::fromUtf8(reinterpret_cast<const char*>(s.data()),int(s.size()));}
std::string utf8(const QString& s){auto b=s.toUtf8();return std::string(b.data(),size_t(b.size()));}
J request(J q){auto encoded=q.dump();const auto* result=host?host(encoded.c_str()):nullptr;try{return result?J::parse(result):J{{"error","Editor is unavailable."}};}catch(const std::exception& e){return J{{"error",e.what()}};}}
void check(const J& r,QWidget* parent){if(r.contains("error")&&!r["error"].get<std::string>().empty())QMessageBox::warning(parent,"Compile",qs(r["error"].get<std::string>()));}
// Palette and style policy from the pinned TB application, matched to editor theme.
QPalette darkPalette()
{
  const auto button = QColor{35, 35, 35};
  const auto text = QColor{207, 207, 207};
  const auto highlight = QColor{62, 112, 205};

  // Build an initial palette based on the button color
  auto palette = QPalette{button};

  // Window colors
  palette.setColor(QPalette::Active, QPalette::Window, QColor{50, 50, 50});
  palette.setColor(QPalette::Inactive, QPalette::Window, QColor{40, 40, 40});
  palette.setColor(QPalette::Disabled, QPalette::Window, QColor{50, 50, 50}.darker(200));

  // List box backgrounds, text entry backgrounds, menu backgrounds
  palette.setColor(QPalette::Base, button.darker(130));

  // Button text
  palette.setColor(QPalette::Active, QPalette::ButtonText, text);
  palette.setColor(QPalette::Inactive, QPalette::ButtonText, text);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, text.darker(200));

  // WindowText is supposed to be against QPalette::Window
  palette.setColor(QPalette::Active, QPalette::WindowText, text);
  palette.setColor(QPalette::Inactive, QPalette::WindowText, text);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, text.darker(200));

  // Menu text, text edit text, table cell text
  palette.setColor(QPalette::Active, QPalette::Text, text.darker(115));
  palette.setColor(QPalette::Inactive, QPalette::Text, text.darker(115));

  // Disabled menu item text color
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor{102, 102, 102});

  // Disabled menu item text shadow
  palette.setColor(QPalette::Disabled, QPalette::Light, button.darker(200));

  // Highlight (selected list box row, selected grid cell background, selected tab text
  palette.setColor(QPalette::Active, QPalette::Highlight, highlight);
  palette.setColor(QPalette::Inactive, QPalette::Highlight, highlight);
  palette.setColor(QPalette::Disabled, QPalette::Highlight, highlight);

  return palette;
}

bool darkTheme=false;
void applyTheme(bool dark){
 static const auto systemStyle=QApplication::style()->objectName();
 static const auto systemPalette=QApplication::palette();
 qApp->setStyle(new QProxyStyle(dark?QStringLiteral("Fusion"):systemStyle));
 qApp->setPalette(dark?darkPalette():systemPalette);
 qApp->styleHints()->setColorScheme(dark?Qt::ColorScheme::Dark:Qt::ColorScheme::Light);
 darkTheme=dark;
}
class CompileDialog;
class ProfileList:public ControlListBox {
 CompileDialog& owner;
public:
 ProfileList(CompileDialog& d,QWidget* p):ControlListBox{"Click the '+' button to create a compilation profile.",true,p},owner(d){}
 void rebuild(){reload();}
 void refresh(){updateItems();}
private:
 size_t itemCount()const override;
 ControlListBoxItemRenderer* createItemRenderer(QWidget*,size_t)override;
};
class TaskList:public ControlListBox {
 CompileDialog& owner;
public:
 TaskList(CompileDialog& d,QWidget* p):ControlListBox{"Click the '+' button to create a task.",QMargins{},false,p},owner(d){}
 void rebuild(){reload();}
private:
 size_t itemCount()const override;
 ControlListBoxItemRenderer* createItemRenderer(QWidget*,size_t)override;
};
class CompileDialog:public QDialog {
public:
 J model=J::array(),engines=J::array(),snapshot;
 int selected=-1;
 bool rebuilding=false;
 std::string gameKey,displayedLog;
 unsigned long long revision=0;
 ProfileList* profiles=nullptr;
 TaskList* tasks=nullptr;
 QLineEdit* name=nullptr;
 MultiCompletionLineEdit* work=nullptr;
 QStackedWidget* editor=nullptr;
 QTextEdit* output=nullptr;
 QPushButton *compile=nullptr,*test=nullptr,*stop=nullptr;
 QToolButton *removeProfile=nullptr,*removeTask=nullptr,*up=nullptr,*down=nullptr;
 QLabel* running=nullptr;
 CompileDialog();
 J* profile(){return selected>=0&&selected<int(model.size())?&model[size_t(selected)]:nullptr;}
 J* taskArray(){auto p=profile();return p?&(*p)["tasks"]:nullptr;}
 void commit(){if(rebuilding)return;auto r=request({{"action","profiles"},{"profiles",model},{"selected",selected}});revision=r.value("revision",revision);check(r,this);}
 void pull(bool force=false);
 void select(int i);
 void rebuildTasks(int row=-1){tasks->rebuild();if(row>=0&&row<tasks->count())tasks->setCurrentRow(row);updateButtons();}
 void updateButtons();
 void profileMenu(size_t i,const QPoint& pos);
 void taskMenu(size_t i,const QPoint& pos);
 void addTask();
 void mutateTask(const std::string& operation,int index);
 void showImport();
 void showSettings();
 void showLaunch();
 void run(bool dry){commit();auto r=request({{"action",dry?"test":"run"},{"selected",selected}});check(r,this);pull();}
 void complete(MultiCompletionLineEdit* edit,bool workdir=false){
  QStringList names;const auto& vars=snapshot.value(workdir?"workdirVariables":"taskVariables",J::object());
  for(auto it=vars.begin();it!=vars.end();++it)names<<qs("${"+it.key()+"}");
  auto* completer=new QCompleter(names,edit);completer->setCaseSensitivity(Qt::CaseInsensitive);edit->setMultiCompleter(completer);edit->setWordDelimiters(QRegularExpression{"\\$"},QRegularExpression{"\\}"});
 }
protected:
 void closeEvent(QCloseEvent* e)override {
  auto s=request({{"action","state"}});
  if(s.value("running",false)&&QMessageBox::warning(this,"Warning","Closing this dialog will stop the running compilation. Are you sure?",QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)!=QMessageBox::Yes){e->ignore();return;}
  if(s.value("running",false))request({{"action","stop"}});commit();request({{"action","closed"}});e->accept();
 }
 void keyPressEvent(QKeyEvent* e)override{if(e->key()==Qt::Key_Escape)close();else QDialog::keyPressEvent(e);}
};
class ProfileItem:public ControlListBoxItemRenderer {
 CompileDialog& owner;size_t index;ElidedLabel *title,*count;
public:
 ProfileItem(CompileDialog& d,size_t i,QWidget* p):ControlListBoxItemRenderer{p},owner(d),index(i){
  setContextMenuPolicy(Qt::CustomContextMenu);title=new ElidedLabel{"",Qt::ElideRight};count=new ElidedLabel{"",Qt::ElideMiddle};setEmphasizedStyle(title);setInfoStyle(count);
  auto* l=new QVBoxLayout;l->setContentsMargins(QMargins{});l->setSpacing(0);l->addWidget(title);l->addWidget(count);setLayout(l);
  connect(this,&QWidget::customContextMenuRequested,this,[this](const QPoint& p){owner.profileMenu(index,mapToGlobal(p));});updateItem();
 }
 void updateItem()override {if(index>=owner.model.size())return;auto& p=owner.model[index];title->setText(qs(p.value("name",std::string{})));count->setText(QString::number(p["tasks"].size())+" tasks");}
};
size_t ProfileList::itemCount()const{return owner.model.size();}
ControlListBoxItemRenderer* ProfileList::createItemRenderer(QWidget* p,size_t i){return new ProfileItem(owner,i,p);}
class TaskItem:public ControlListBoxItemRenderer {
 CompileDialog& owner;size_t index;
 J& task(){return (*owner.taskArray())[index];}
 MultiCompletionLineEdit* field(QFormLayout* f,const char* label,const char* key,const char* tooltip){
  auto* edit=new MultiCompletionLineEdit;edit->setFont(Fonts::fixedWidthFont());edit->setText(qs(task().value(key,std::string{})));edit->setToolTip(tooltip);owner.complete(edit);f->addRow(label,edit);
  connect(edit,&QLineEdit::textChanged,edit,[this,key](const QString& text){task()[key]=utf8(text);owner.commit();});return edit;
 }
 void flag(QFormLayout* f,const char* label,const char* key){auto* b=new QCheckBox(label);b->setChecked(task().value(key,false));f->addRow("",b);connect(b,&QCheckBox::clicked,b,[this,key](bool v){task()[key]=v;owner.commit();});}
public:
 TaskItem(CompileDialog& d,size_t i,QWidget* parent):ControlListBoxItemRenderer{parent},owner(d),index(i){
  setContextMenuPolicy(Qt::CustomContextMenu);auto type=task().at("type").get<std::string>();
  const auto title=type=="export"?"Export Map":type=="copy"?"Copy Files":type=="rename"?"Rename File":type=="delete"?"Delete Files":type=="tool"?"Run Tool":"Launch Engine";
  auto* panel=new TitledPanel{title};auto* l=new QVBoxLayout;l->setContentsMargins(0,0,0,0);l->setSpacing(0);l->addWidget(panel);l->addWidget(new BorderLine);setLayout(l);
  auto* enabled=new QCheckBox;enabled->setChecked(task().value("enabled",true));enabled->setToolTip("Whether to include this task when running the compile profile");
  auto* line=new QHBoxLayout;line->setContentsMargins(0,0,0,0);line->addSpacing(LayoutConstants::NarrowHMargin);line->addWidget(enabled,0,Qt::AlignVCenter);line->addSpacing(LayoutConstants::NarrowHMargin);
  auto* f=new QFormLayout;f->setContentsMargins(8,8,8,8);f->setVerticalSpacing(4);f->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);line->addLayout(f,1);panel->getPanel()->setLayout(line);
  connect(enabled,&QCheckBox::clicked,enabled,[this](bool v){task()["enabled"]=v;owner.commit();});
  if(type=="export"){
   field(f,"File Path","target","The path of the exported file.\nVariables are allowed.");
   auto* strip=new QLineEdit(qs(task().value("stripEntityPattern",std::string{})));strip->setPlaceholderText("enter GLOB pattern");strip->setToolTip("Set a GLOB pattern to strip any entities with a matching classname.");f->addRow("Strip Entities",strip);
   connect(strip,&QLineEdit::textChanged,strip,[this](const QString& t){if(t.isEmpty())task().erase("stripEntityPattern");else task()["stripEntityPattern"]=utf8(t);owner.commit();});
   std::string classname;for(const auto& p:task().value("entityToAdd",J::array()))if(p.value("key",std::string{})=="classname")classname=p.value("value",std::string{});
   auto* add=new QLineEdit(qs(classname));add->setPlaceholderText("enter classname");add->setToolTip("Set the classname of an entity that will be added to the map. The entities' origin and angles will be set to the camera position and view direction, respectively.");f->addRow("Add Entity",add);
   connect(add,&QLineEdit::textChanged,add,[this](const QString& t){if(t.isEmpty())task().erase("entityToAdd");else{auto a=task().value("entityToAdd",J::array());bool found=false;for(auto& p:a)if(p.value("key",std::string{})=="classname"){p["value"]=utf8(t);found=true;}if(!found)a.push_back({{"key","classname"},{"value",utf8(t)}});task()["entityToAdd"]=a;}owner.commit();});
   flag(f,"Strip TrenchBroom specific entity properties","stripTbProperties");
  }else if(type=="copy"||type=="rename"){
   field(f,"Source File Path","source",type=="copy"?"The file(s) to copy.\nWildcards (*, ?) are allowed in the filename.\nVariables are allowed.":"The file to rename or move.\nWildcards are not supported.\nVariables are allowed.");
   field(f,type=="copy"?"Target Directory Path":"Target File Path","target",type=="copy"?"The directory to copy the files to.\nThe directory is recursively created if it does not exist.\nExisting files are overwritten without prompt.\nVariables are allowed.":"The new path for the file.\nThe path must end in a filename.\nThe containing directory is recursively created if it does not exist.\nExisting files are overwritten without prompt.\nVariables are allowed.");
  }else if(type=="delete")field(f,"File Path","target","The file(s) to delete.\nWildcards (*, ?) are allowed in the filename.\nVariables are allowed.");
  else if(type=="tool"){
   auto* edit=new MultiCompletionLineEdit;edit->setFont(Fonts::fixedWidthFont());edit->setText(qs(task().value("tool",std::string{})));edit->setToolTip("The absolute path to the executable of the tool that should be run.\nVariables are allowed.");owner.complete(edit);
   auto* browse=new QPushButton("...");browse->setToolTip("Click to browse");auto* row=new QHBoxLayout;row->setContentsMargins(0,0,0,0);row->setSpacing(LayoutConstants::NarrowHMargin);row->addWidget(edit,1);row->addWidget(browse);f->addRow("Tool Path",row);
   connect(edit,&QLineEdit::textChanged,edit,[this](const QString& t){task()["tool"]=utf8(t);owner.commit();});connect(browse,&QPushButton::clicked,browse,[this,edit]{auto p=QFileDialog::getOpenFileName(&owner,"Choose Tool",edit->text());if(!p.isEmpty())edit->setText(p);});
   field(f,"Parameters","parameters","The parameters that should be passed to the tool when it is executed.\nVariables are allowed.");flag(f,"Stop on nonzero error code","treatNonZeroResultCodeAsError");
  }else{
   auto* box=new QComboBox;std::string id=task().value("engineProfileId",std::string{});int row=-1;
   for(size_t k=0;k<owner.engines.size();++k){auto& e=owner.engines[k];box->addItem(qs(e.value("name",std::string{})),qs(e.value("id",std::string{})));if(e.value("id",std::string{})==id)row=int(k);}
   box->setCurrentIndex(row);box->setToolTip("The Launch Engine profile to start.");f->addRow("Engine Profile",box);connect(box,qOverload<int>(&QComboBox::currentIndexChanged),box,[this,box](int i){if(i>=0){task()["engineProfileId"]=utf8(box->itemData(i).toString());owner.commit();}});flag(f,"Stop on launch failure","treatLaunchFailureAsError");
  }
  connect(this,&QWidget::customContextMenuRequested,this,[this](const QPoint& p){owner.taskMenu(index,mapToGlobal(p));});
 }
};
size_t TaskList::itemCount()const{auto p=owner.profile();return p?p->at("tasks").size():0;}
ControlListBoxItemRenderer* TaskList::createItemRenderer(QWidget* p,size_t i){return new TaskItem(owner,i,p);}
CompileDialog::CompileDialog(){
 setWindowTitle("Compile");setWindowIconTB(this);setMinimumSize(600,300);resize(800,600);setModal(false);
 auto* manager=new QWidget;setBaseWindowColor(manager);auto* listPanel=new TitledPanel("Profiles");auto* detailPanel=new TitledPanel("Details");
 profiles=new ProfileList(*this,listPanel->getPanel());auto* add=createBitmapButton("Add.svg","Add profile");removeProfile=createBitmapButton("Remove.svg","Remove the selected profile");
 add->setObjectName("CompilationProfileManager_AddProfileButton");removeProfile->setObjectName("CompilationProfileManager_RemoveProfileButton");
 auto* import=new QPushButton("Import TrenchBroom profiles...");import->setToolTip("Import TrenchBroom profiles...");import->setObjectName("ImportTrenchBroomProfiles");
 auto* listLayout=new QVBoxLayout;listLayout->setContentsMargins(0,0,0,0);listLayout->setSpacing(0);listLayout->addWidget(profiles,1);listLayout->addWidget(import);listLayout->addWidget(new BorderLine);listLayout->addLayout(createMiniToolBarLayout(add,removeProfile));listPanel->getPanel()->setLayout(listLayout);
 editor=new QStackedWidget;editor->addWidget(createEmptyWidget("Select a compilation profile",editor));
 auto* container=new QWidget(editor);auto* upper=new QWidget(container);setDefaultWindowColor(upper);name=new QLineEdit;work=new MultiCompletionLineEdit;work->setFont(Fonts::fixedWidthFont());work->setToolTip("A working directory for the compilation profile.\nVariables are allowed.");
 auto* head=new QFormLayout;head->setContentsMargins(LayoutConstants::MediumHMargin,8,LayoutConstants::MediumHMargin,8);head->setHorizontalSpacing(LayoutConstants::MediumHMargin);head->setVerticalSpacing(0);head->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);head->addRow("Name",name);head->addRow("Working Directory",work);upper->setLayout(head);
 tasks=new TaskList(*this,container);auto* plus=createBitmapButton("Add.svg","Add task");removeTask=createBitmapButton("Remove.svg","Remove the selected task");up=createBitmapButton("Up.svg","Move the selected task up");down=createBitmapButton("Down.svg","Move the selected task down");
 auto* editLayout=new QVBoxLayout;editLayout->setContentsMargins(0,0,0,0);editLayout->setSpacing(0);editLayout->addWidget(upper);editLayout->addWidget(new BorderLine);editLayout->addWidget(tasks,1);editLayout->addWidget(new BorderLine);editLayout->addLayout(createMiniToolBarLayout(plus,removeTask,up,down));container->setLayout(editLayout);editor->addWidget(container);
 auto* stackedLayout=new QHBoxLayout;stackedLayout->setContentsMargins(0,0,0,0);stackedLayout->setSpacing(0);stackedLayout->addWidget(editor);auto* stacked=new QWidget;stacked->setLayout(stackedLayout);
 auto* detailLayout=new QVBoxLayout;detailLayout->setContentsMargins(0,0,0,0);detailLayout->setSpacing(0);detailLayout->addWidget(stacked);detailPanel->getPanel()->setLayout(detailLayout);
 auto* outer=new QHBoxLayout;outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);outer->addWidget(listPanel);outer->addWidget(new BorderLine{BorderLine::Direction::Vertical});outer->addWidget(detailPanel,1);manager->setLayout(outer);listPanel->setMinimumSize(200,200);
 output=new QTextEdit;output->setReadOnly(true);output->setFont(Fonts::fixedWidthFont());output->setContextMenuPolicy(Qt::CustomContextMenu);connect(output,&QWidget::customContextMenuRequested,output,[this](const QPoint& p){auto* menu=output->createStandardContextMenu();menu->addSeparator();menu->addAction("Clear",this,[this]{request({{"action","clearLog"}});displayedLog.clear();output->clear();});menu->popup(output->mapToGlobal(p));});
 auto* splitter=new Splitter(Qt::Vertical);splitter->addWidget(manager);splitter->addWidget(output);splitter->setSizes({2,1});
 auto* buttons=new QDialogButtonBox;auto* launch=buttons->addButton("Launch...",QDialogButtonBox::NoRole);stop=buttons->addButton("Stop",QDialogButtonBox::NoRole);test=buttons->addButton("Test",QDialogButtonBox::NoRole);compile=buttons->addButton("Compile",QDialogButtonBox::NoRole);auto* closeButton=buttons->addButton("Close",QDialogButtonBox::RejectRole);compile->setDefault(true);
 running=new QLabel;running->setAlignment(Qt::AlignRight);auto* footer=new QHBoxLayout;footer->setContentsMargins(0,0,0,0);footer->setSpacing(8);footer->addWidget(launch,0,Qt::AlignVCenter);footer->addWidget(running,1,Qt::AlignVCenter);footer->addWidget(buttons);
 auto* layout=new QVBoxLayout;layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);layout->addWidget(splitter,1);layout->addLayout(wrapDialogButtonBox(footer));insertTitleBarSeparator(layout);setLayout(layout);
 connect(profiles,&ControlListBox::itemSelectionChanged,this,[this]{if(!rebuilding)select(profiles->selectedRow());});
 connect(tasks,&ControlListBox::itemSelectionChanged,this,[this]{updateButtons();});
 connect(name,&QLineEdit::textChanged,this,[this](const QString& t){if(!rebuilding&&profile()){(*profile())["name"]=utf8(t);profiles->refresh();commit();}});
 connect(work,&QLineEdit::textChanged,this,[this](const QString& t){if(!rebuilding&&profile()){(*profile())["workdir"]=utf8(t);commit();}});
 connect(add,&QAbstractButton::clicked,this,[this]{model.push_back({{"name","unnamed"},{"workdir","${MAP_DIR_PATH}"},{"tasks",J::array()},{"trenchBroom",true}});rebuilding=true;profiles->rebuild();rebuilding=false;profiles->setCurrentRow(int(model.size()-1));commit();});
 connect(removeProfile,&QAbstractButton::clicked,this,[this]{if(selected>=0){model.erase(model.begin()+selected);auto next=std::min(selected,int(model.size())-1);rebuilding=true;profiles->rebuild();rebuilding=false;profiles->setCurrentRow(next);select(next);commit();}});
 connect(plus,&QAbstractButton::clicked,this,[this]{addTask();});connect(removeTask,&QAbstractButton::clicked,this,[this]{mutateTask("remove",tasks->selectedRow());});connect(up,&QAbstractButton::clicked,this,[this]{mutateTask("up",tasks->selectedRow());});connect(down,&QAbstractButton::clicked,this,[this]{mutateTask("down",tasks->selectedRow());});
 connect(import,&QPushButton::clicked,this,[this]{showImport();});connect(compile,&QPushButton::clicked,this,[this]{run(false);});connect(test,&QPushButton::clicked,this,[this]{run(true);});connect(stop,&QPushButton::clicked,this,[]{request({{"action","stop"}});});connect(closeButton,&QPushButton::clicked,this,&QDialog::close);connect(launch,&QPushButton::clicked,this,[this]{showLaunch();});
 pull(true);auto* timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{pull();});timer->start(100);
}
void CompileDialog::select(int i){selected=i;rebuilding=true;auto* p=profile();editor->setCurrentIndex(p?1:0);name->setText(p?qs(p->value("name",std::string{})):QString{});work->setText(p?qs(p->value("workdir",std::string{})):QString{});complete(work,true);tasks->rebuild();rebuilding=false;updateButtons();request({{"action","select"},{"selected",selected}});}
void CompileDialog::pull(bool force){
 auto s=request({{"action","state"}});if(s.contains("error")&&!s.contains("profiles"))return;
 if(s.value("darkTheme",false)!=darkTheme)applyTheme(s.value("darkTheme",false));
 auto rev=s.value("revision",0ULL);auto key=s.value("game",std::string{});snapshot=s;
 if(force||rev!=revision||key!=gameKey){
  auto previous=selected;model=s.value("profiles",J::array());engines=s.value("engines",J::array());revision=rev;gameKey=key;
  selected=s.value("selected",previous);if(selected<0||selected>=int(model.size()))selected=model.empty()?-1:0;
  rebuilding=true;profiles->rebuild();profiles->setCurrentRow(selected);rebuilding=false;select(selected);
 }
 auto log=s.value("log",std::string{});if(log!=displayedLog){bool atEnd=output->verticalScrollBar()->value()>=output->verticalScrollBar()->maximum();if(log.rfind(displayedLog,0)==0){QTextCursor cursor(output->document());cursor.movePosition(QTextCursor::End);cursor.insertText(qs(log.substr(displayedLog.size())));}else output->setPlainText(qs(log));displayedLog=log;if(atEnd)output->verticalScrollBar()->setValue(output->verticalScrollBar()->maximum());}
 running->setText(s.value("running",false)?qs("Running "+s.value("runningName",std::string{})):QString{});updateButtons();
}
void CompileDialog::updateButtons(){auto* p=profile();bool active=snapshot.value("running",false);compile->setEnabled(!active&&p&&!(*p)["tasks"].empty());test->setEnabled(compile->isEnabled());stop->setEnabled(active);removeProfile->setEnabled(p);auto* a=taskArray();auto row=tasks->selectedRow();removeTask->setEnabled(a&&row>=0);up->setEnabled(a&&row>0);down->setEnabled(a&&row>=0&&row+1<int(a->size()));}
void CompileDialog::profileMenu(size_t i,const QPoint& p){if(i>=model.size())return;QMenu m;auto* duplicate=m.addAction("Duplicate");auto* remove=m.addAction("Remove");auto* a=m.exec(p);if(!a)return;if(a==duplicate){model.push_back(model[i]);selected=int(model.size()-1);}else if(a==remove){model.erase(model.begin()+i);selected=std::min(int(i),int(model.size())-1);}rebuilding=true;profiles->rebuild();rebuilding=false;profiles->setCurrentRow(selected);select(selected);commit();}
void CompileDialog::taskMenu(size_t i,const QPoint& p){auto* a=taskArray();if(!a||i>=a->size())return;QMenu m;auto* upAction=m.addAction("Move Up");upAction->setEnabled(i>0);auto* downAction=m.addAction("Move Down");downAction->setEnabled(i+1<a->size());m.addSeparator();auto* duplicate=m.addAction("Duplicate");auto* remove=m.addAction("Remove");auto* action=m.exec(p);if(action)mutateTask(action==upAction?"up":action==downAction?"down":action==duplicate?"duplicate":"remove",int(i));}
void CompileDialog::mutateTask(const std::string& op,int i){auto* a=taskArray();if(!a||i<0||i>=int(a->size()))return;if(op=="up"&&i>0){std::swap((*a)[i],(*a)[i-1]);--i;}else if(op=="down"&&i+1<int(a->size())){std::swap((*a)[i],(*a)[i+1]);++i;}else if(op=="duplicate"){auto copy=(*a)[i];a->insert(a->begin()+i+1,std::move(copy));++i;}else if(op=="remove"){a->erase(a->begin()+i);i=std::min(i,int(a->size())-1);}rebuildTasks(i);profiles->refresh();commit();}
void CompileDialog::addTask(){auto* a=taskArray();if(!a)return;QMenu menu;QStringList names={"Export Map","Copy Files","Rename File","Delete Files","Run Tool","Launch Engine"};for(const auto& name:names)menu.addAction(name);auto* chosen=menu.exec(QCursor::pos());if(!chosen)return;int type=names.indexOf(chosen->text());J t={{"enabled",true}};if(type==0)t.update({{"type","export"},{"target","${WORK_DIR_PATH}/${MAP_BASE_NAME}-compile.map"},{"stripTbProperties",false}});else if(type==1||type==2)t.update({{"type",type==1?"copy":"rename"},{"source",""},{"target",""}});else if(type==3)t.update({{"type","delete"},{"target",""}});else if(type==4)t.update({{"type","tool"},{"tool",""},{"parameters",""},{"treatNonZeroResultCodeAsError",false}});else t.update({{"type","launchEngine"},{"engineProfileId",engines.empty()?"":engines.front().value("id",std::string{})},{"treatLaunchFailureAsError",false}});auto row=tasks->selectedRow();int at=row<0?int(a->size()):row+1;a->insert(a->begin()+at,std::move(t));rebuildTasks(at);profiles->refresh();commit();}
void CompileDialog::showImport(){
 auto* dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Import TrenchBroom profiles");dialog->resize(660,520);
 auto* l=new QVBoxLayout(dialog);auto* message=new QLabel("Select a local CompilationProfiles.cfg from TrenchBroom. Imported steps are saved without running them.");message->setWordWrap(true);l->addWidget(message);
 auto* path=new QLineEdit;auto* browse=new QPushButton("Browse...");auto* row=new QHBoxLayout;row->addWidget(path,1);row->addWidget(browse);l->addLayout(row);
 auto* details=new QTextEdit;details->setReadOnly(true);l->addWidget(details,1);auto* missing=new QWidget;auto* missingForm=new QFormLayout(missing);l->addWidget(missing);
 auto* buttons=new QHBoxLayout;auto* review=new QPushButton("Review import");auto* import=new QPushButton("Import profiles");auto* cancel=new QPushButton("Cancel");buttons->addWidget(review);buttons->addWidget(import);buttons->addWidget(cancel);l->addLayout(buttons);import->setEnabled(false);
 auto inputs=std::make_shared<std::map<std::string,QLineEdit*>>();
 connect(browse,&QPushButton::clicked,dialog,[dialog,path]{auto p=QFileDialog::getOpenFileName(dialog,"Choose CompilationProfiles.cfg",path->text(),"Profiles (*.cfg);;All files (*)");if(!p.isEmpty())path->setText(p);});
 connect(path,&QLineEdit::textChanged,dialog,[import]{import->setEnabled(false);});
 connect(review,&QPushButton::clicked,dialog,[dialog,path,details,import,missingForm,inputs]{
  auto r=request({{"action","reviewImport"},{"path",utf8(path->text())}});check(r,dialog);QString text;
  for(const auto& p:r.value("profiles",J::array()))text+=qs(p.value("name",std::string{}))+" ("+QString::number(p["tasks"].size())+" steps)\n";
  for(const auto& notice:r.value("notices",std::vector<std::string>{}))text+=qs(notice)+"\n";details->setPlainText(text);
  while(missingForm->rowCount())missingForm->removeRow(0);inputs->clear();auto vars=r.value("missingVariables",J::object());
  for(auto it=vars.begin();it!=vars.end();++it){auto* e=new QLineEdit(qs(it.value().get<std::string>()));missingForm->addRow(qs(it.key()),e);(*inputs)[it.key()]=e;}
  import->setEnabled(!r.value("profiles",J::array()).empty());
 });
 connect(import,&QPushButton::clicked,dialog,[this,dialog,inputs]{J vars=J::object();for(const auto& v:*inputs)vars[v.first]=utf8(v.second->text());auto r=request({{"action","import"},{"variables",vars}});check(r,dialog);if(!r.contains("error")){pull(true);dialog->accept();}});
 connect(cancel,&QPushButton::clicked,dialog,&QDialog::reject);dialog->show();
}
void CompileDialog::showSettings(){
 auto s=request({{"action","state"}});auto* d=new QDialog(this);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle("Compilation settings");d->resize(760,560);auto* layout=new QVBoxLayout(d);auto* tabs=new QTabWidget;layout->addWidget(tabs,1);
 auto* paths=new QWidget;auto* form=new QFormLayout(paths);auto* root=new QLineEdit(qs(s.value("gameRoot",std::string{})));auto* base=new QLineEdit(qs(s.value("baseMod",std::string{})));auto* mods=new QPlainTextEdit;mods->setPlaceholderText("One enabled mod per line, in map load order. The base mod is added first.");for(const auto& m:s.value("enabledMods",std::vector<std::string>{}))mods->appendPlainText(qs(m));
 form->addRow("Game directory",root);form->addRow("Default mod",base);form->addRow("Enabled mods",mods);
 auto* variables=new QTableWidget(0,2);variables->setHorizontalHeaderLabels({"Tool / variable","Path / value"});variables->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);auto vars=s.value("variables",J::object());for(auto it=vars.begin();it!=vars.end();++it){int r=variables->rowCount();variables->insertRow(r);variables->setItem(r,0,new QTableWidgetItem(qs(it.key())));variables->setItem(r,1,new QTableWidgetItem(qs(it.value().get<std::string>())));}
 form->addRow("Compilation tools",variables);auto* addVar=new QPushButton("Add tool / variable");form->addRow("",addVar);connect(addVar,&QPushButton::clicked,d,[variables]{int r=variables->rowCount();variables->insertRow(r);variables->setItem(r,0,new QTableWidgetItem);variables->setItem(r,1,new QTableWidgetItem);variables->setCurrentCell(r,0);});tabs->addTab(paths,"Paths and tools");
 auto* engineTab=new QWidget;auto* eLayout=new QVBoxLayout(engineTab);auto* table=new QTableWidget(0,4);table->setHorizontalHeaderLabels({"ID","Name","Path","Parameters"});table->setColumnHidden(0,true);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
 for(const auto& e:s.value("engines",J::array())){auto r=table->rowCount();table->insertRow(r);const char* keys[]={"id","name","path","parameters"};for(int c=0;c<4;++c)table->setItem(r,c,new QTableWidgetItem(qs(e.value(keys[c],std::string{}))));}
 eLayout->addWidget(table,1);auto* eButtons=new QHBoxLayout;auto* addEngine=createBitmapButton("Add.svg","Add profile");auto* removeEngine=createBitmapButton("Remove.svg","Remove the selected profile");auto* browse=new QPushButton("Browse...");eButtons->addWidget(addEngine);eButtons->addWidget(removeEngine);eButtons->addWidget(browse);eButtons->addStretch();eLayout->addLayout(eButtons);tabs->addTab(engineTab,"Engine profiles");
 connect(addEngine,&QAbstractButton::clicked,d,[table]{int r=table->rowCount();table->insertRow(r);for(int c=0;c<4;++c)table->setItem(r,c,new QTableWidgetItem(c==0?QUuid::createUuid().toString(QUuid::WithoutBraces):c==1?"unnamed":""));table->setCurrentCell(r,1);});
 connect(removeEngine,&QAbstractButton::clicked,d,[table]{if(table->currentRow()>=0)table->removeRow(table->currentRow());});connect(browse,&QPushButton::clicked,d,[d,table]{auto r=table->currentRow();if(r<0)return;auto p=QFileDialog::getOpenFileName(d,"Choose Game Engine");if(!p.isEmpty())table->item(r,2)->setText(p);});
 auto* presets=new QWidget;auto* pLayout=new QVBoxLayout(presets);auto* presetList=new QListWidget;auto available=s.value("presets",J::array());for(const auto& p:available)presetList->addItem(qs(p.value("name",std::string{})));pLayout->addWidget(presetList);auto* addPreset=new QPushButton("Add selected bundled preset");pLayout->addWidget(addPreset);tabs->addTab(presets,"Bundled presets");connect(addPreset,&QPushButton::clicked,d,[this,presetList,available]{int i=presetList->currentRow();if(i<0)return;model.push_back(available[size_t(i)]);selected=int(model.size()-1);commit();pull(true);});
 auto* box=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(box);
 connect(box,&QDialogButtonBox::accepted,d,[this,d,root,base,mods,variables,table]{J vars=J::object(),eng=J::array(),enabled=J::array();for(int r=0;r<variables->rowCount();++r)if(variables->item(r,0)&&variables->item(r,1)&&!variables->item(r,0)->text().isEmpty())vars[utf8(variables->item(r,0)->text())]=utf8(variables->item(r,1)->text());for(int r=0;r<table->rowCount();++r){J e;const char* keys[]={"id","name","path","parameters"};for(int c=0;c<4;++c)e[keys[c]]=table->item(r,c)?utf8(table->item(r,c)->text()):std::string{};if(e["id"]=="")e["id"]=utf8(QUuid::createUuid().toString(QUuid::WithoutBraces));eng.push_back(e);}for(const auto& m:mods->toPlainText().split('\n'))if(!m.trimmed().isEmpty())enabled.push_back(utf8(m.trimmed()));auto res=request({{"action","settings"},{"gameRoot",utf8(root->text())},{"baseMod",utf8(base->text())},{"enabledMods",enabled},{"variables",vars},{"engines",eng}});check(res,d);if(!res.contains("error")){pull(true);d->accept();}});connect(box,&QDialogButtonBox::rejected,d,&QDialog::reject);d->show();
}
void CompileDialog::showLaunch(){
 pull();auto* d=new QDialog(this);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle("Launch Engine");setWindowIconTB(d);
 auto* list=new QListWidget;list->setMinimumSize(250,280);auto* header=new QLabel("Launch Engine");setHeaderStyle(header);auto* message=new QLabel("Select a game engine from the list on the right and edit the commandline parameters in the text box below. You can use variables to refer to the map name and other values.");message->setWordWrap(true);auto* configure=new QPushButton("Configure engines...");auto* label=new QLabel("Parameters");setEmphasizedStyle(label);auto* parameters=new MultiCompletionLineEdit;parameters->setFont(Fonts::fixedWidthFont());complete(parameters);
 auto* left=new QVBoxLayout;left->setContentsMargins(0,0,0,0);left->setSpacing(0);left->addSpacing(20);left->addWidget(header);left->addSpacing(20);left->addWidget(message);left->addSpacing(10);left->addWidget(configure,0,Qt::AlignHCenter);left->addStretch();left->addWidget(label);left->addSpacing(LayoutConstants::NarrowVMargin);left->addWidget(parameters);left->addSpacing(20);
 auto* middle=new QHBoxLayout;middle->setContentsMargins(0,0,0,0);middle->setSpacing(0);middle->addSpacing(20);middle->addLayout(left,1);middle->addSpacing(20);middle->addWidget(new BorderLine{BorderLine::Direction::Vertical});middle->addWidget(list);
 auto* buttons=new QDialogButtonBox;auto* launch=buttons->addButton("Launch",QDialogButtonBox::AcceptRole);auto* close=buttons->addButton("Close",QDialogButtonBox::RejectRole);auto* l=new QVBoxLayout(d);l->setContentsMargins(0,0,0,0);l->setSpacing(0);auto* indicator=new QLabel(qs(snapshot.value("gameName",std::string{})));l->addWidget(indicator);l->addWidget(new BorderLine);l->addLayout(middle,1);l->addLayout(wrapDialogButtonBox(buttons));d->resize(660,360);
 auto engineState=std::make_shared<J>(J::array());auto rebuild=[this,list,parameters,launch,engineState]{auto state=request({{"action","state"}});auto next=state.value("engines",J::array());if(next==*engineState)return;int index=list->currentRow();*engineState=next;list->clear();for(const auto& e:next){auto* item=new QListWidgetItem(qs(e.value("name",std::string{}))+"\n"+qs(e.value("path",std::string{})),list);item->setSizeHint(QSize(250,36));}list->setCurrentRow(next.empty()?-1:std::clamp(index,0,int(next.size())-1));parameters->setEnabled(!next.empty());launch->setEnabled(!next.empty());};
 connect(list,&QListWidget::currentRowChanged,d,[parameters,engineState](int i){parameters->setText(i>=0&&i<int(engineState->size())?qs((*engineState)[i].value("parameters",std::string{})):QString{});});
 connect(parameters,&QLineEdit::textEdited,d,[list,engineState](const QString& t){int i=list->currentRow();if(i>=0)(*engineState)[i]["parameters"]=utf8(t);});
 auto start=[d,list,parameters,engineState]{int i=list->currentRow();if(i<0)return;auto e=(*engineState)[i];e["parameters"]=utf8(parameters->text());check(request({{"action","launchEngine"},{"engine",e}}),d);};connect(launch,&QPushButton::clicked,d,start);connect(parameters,&QLineEdit::returnPressed,d,start);connect(list,&QListWidget::itemDoubleClicked,d,[start](QListWidgetItem*){start();});connect(close,&QPushButton::clicked,d,&QDialog::close);connect(configure,&QPushButton::clicked,d,[this]{showSettings();});auto* timer=new QTimer(d);connect(timer,&QTimer::timeout,d,rebuild);timer->start(200);rebuild();d->show();
}
std::unique_ptr<CompileDialog> dialog;
std::unique_ptr<QWindow> editorOwner;
}
RB_COMPILE_EXPORT int reyondCompileInitialize(ReyondCompileRequest callback,const char* resources){
 try {host=callback;reyondCompileResources=std::filesystem::u8path(resources);
  auto root=reyondCompileResources.parent_path().parent_path().parent_path();
  auto plugins=root/"tools/compile-ui/plugins";
  if(std::filesystem::is_directory(plugins))QCoreApplication::addLibraryPath(qs(plugins.u8string()));
  if(!qApp){
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
    QGuiApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
    static int argc=1;static char appName[]="ReyondBroom";static char* argv[]={appName,nullptr};ownedApplication=std::make_unique<QApplication>(argc,argv);QApplication::setQuitOnLastWindowClosed(false);}
  QFile style(qs((reyondCompileResources/"stylesheets/base.qss").u8string()));
  if(style.open(QFile::ReadOnly|QFile::Text))qApp->setStyleSheet(QString::fromUtf8(style.readAll()));
  qApp->setStyle(new QProxyStyle{});
  dialog=std::make_unique<CompileDialog>();
  if(auto id=reyondEditorWindow()){
    editorOwner.reset(QWindow::fromWinId(id));dialog->winId();
    if(editorOwner){dialog->windowHandle()->setTransientParent(editorOwner.get());auto origin=editorOwner->mapToGlobal(QPoint{});auto size=editorOwner->size();dialog->move(origin+QPoint((size.width()-dialog->width())/2,(size.height()-dialog->height())/2));}
  }
  return 1;
 }catch(...){return 0;}
}
RB_COMPILE_EXPORT void reyondCompileShow(int mode){if(!dialog)return;dialog->pull();if(mode==1)dialog->showSettings();else if(mode==2)dialog->showLaunch();else if(mode==3){
 auto* box=new QMessageBox(QMessageBox::Warning,"Warning","Closing the editor will stop the running compilation. Are you sure?",QMessageBox::Yes|QMessageBox::No,dialog.get());box->setAttribute(Qt::WA_DeleteOnClose);box->setDefaultButton(QMessageBox::Yes);QObject::connect(box,&QDialog::finished,box,[](int answer){if(answer==QMessageBox::Yes){dialog->hide();request({{"action","exitConfirmed"}});}});box->open();
 }else{dialog->show();dialog->raise();dialog->activateWindow();}}
RB_COMPILE_EXPORT void reyondCompileTick(){if(qApp){QCoreApplication::processEvents(QEventLoop::AllEvents,5);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}}
RB_COMPILE_EXPORT int reyondCompileVisible(){return dialog&&dialog->isVisible();}
RB_COMPILE_EXPORT void reyondCompileShutdown(){fprintf(stderr,"Compile UI: shutdown begin\n");dialog.reset();editorOwner.reset();ownedApplication.reset();host=nullptr;fprintf(stderr,"Compile UI: shutdown complete\n");}

// Developer QA: capture actual native widgets and their DIP geometry from this process.
RB_COMPILE_EXPORT int reyondCompileCapture(const char* directory,int width,int height,int dark){
 if(!dialog)return 0;
 try{
  applyTheme(dark!=0);dialog->resize(width,height);dialog->show();
  QCoreApplication::processEvents();
  auto root=std::filesystem::u8path(directory);std::filesystem::create_directories(root);
  if(!dialog->grab().save(qs((root/"compile.png").u8string())))return 0;
  J widgets=J::array();
  for(auto* w:dialog->findChildren<QWidget*>())if(w->isVisibleTo(dialog.get())){
   auto p=w->mapTo(dialog.get(),QPoint{});J v={{"class",w->metaObject()->className()},{"name",utf8(w->objectName())},{"x",p.x()},{"y",p.y()},{"width",w->width()},{"height",w->height()}};
   if(auto* b=qobject_cast<QAbstractButton*>(w))v["text"]=utf8(b->text());
   if(auto* label=qobject_cast<QLabel*>(w))v["text"]=utf8(label->text());widgets.push_back(v);
  }
  J geometry={{"qt",qVersion()},{"style",utf8(qApp->style()->objectName())},{"dark",dark!=0},{"dpr",dialog->devicePixelRatioF()},{"width",dialog->width()},{"height",dialog->height()},{"font",utf8(qApp->font().toString())},{"widgets",widgets}};
  QFile file(qs((root/"geometry.json").u8string()));if(!file.open(QFile::WriteOnly))return 0;file.write(QByteArray::fromStdString(geometry.dump(2)));return 1;
 }catch(...){return 0;}
}
