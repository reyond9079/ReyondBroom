// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic style discovery hook for the unmodified official TB binary.
// Returning nullptr leaves Qt's actual system/Fusion style selection untouched.
#include <QtWidgets>
class ReferenceProbe final:public QStylePlugin {
 Q_OBJECT
 Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QStyleFactoryInterface" FILE "probe.json")
public:
 QStyle* create(const QString&)override{
  static bool installed=false;
  if(!installed&&!qEnvironmentVariableIsEmpty("RB_REFERENCE_CAPTURE")){
   installed=true;
   QTimer::singleShot(200,[]{
    auto* timer=new QTimer(qApp);
    QObject::connect(timer,&QTimer::timeout,timer,[timer]{
     static bool opening=false,captured=false;
     if(captured)return;
     for(auto* w:QApplication::topLevelWidgets())if(w->windowTitle()=="Compile"&&w->isVisible()){
      captured=true;timer->stop();
      int width=qEnvironmentVariableIntValue("RB_REFERENCE_WIDTH");if(!width)width=800;
      int height=qEnvironmentVariableIntValue("RB_REFERENCE_HEIGHT");if(!height)height=600;
      w->resize(width,height);QCoreApplication::processEvents();
      QDir root(qEnvironmentVariable("RB_REFERENCE_CAPTURE"));root.mkpath(".");
      w->grab().save(root.filePath("compile.png"));
      QJsonArray widgets;
      for(auto* child:w->findChildren<QWidget*>())if(child->isVisibleTo(w)){
       auto p=child->mapTo(w,QPoint{});QJsonObject item{{"class",child->metaObject()->className()},{"name",child->objectName()},{"x",p.x()},{"y",p.y()},{"width",child->width()},{"height",child->height()}};
       if(auto* b=qobject_cast<QAbstractButton*>(child))item["text"]=b->text();if(auto* l=qobject_cast<QLabel*>(child))item["text"]=l->text();widgets.push_back(item);
      }
      QJsonObject geometry{{"qt",qVersion()},{"font",qApp->font().toString()},{"dpr",w->devicePixelRatioF()},{"width",w->width()},{"height",w->height()},{"widgets",widgets}};
      QFile file(root.filePath("geometry.json"));if(file.open(QFile::WriteOnly))file.write(QJsonDocument(geometry).toJson());
      QTimer::singleShot(300,[]{QApplication::quit();});return;
     }
     if(opening)return;
     for(auto* w:QApplication::allWidgets())for(auto* action:w->actions()){
      auto text=action->text();text.remove('&');if(text=="Compile Map..."&&action->isEnabled()){opening=true;action->trigger();return;}
     }
    });timer->start(200);
   });
  }
  return nullptr;
 }
};
#include "probe.moc"
