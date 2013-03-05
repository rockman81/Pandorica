#include "sessiondetailsdialog.h"
#include "session.h"
#include "sessioninfo.h"
#include "sessiondetails.h"
#include "customitemdelegates.h"
#include <Wt/Dbo/QueryModel>
#include <Wt/WTableView>
#include <Wt/WTimer>

using namespace std;
using namespace Wt;
using namespace boost;

SessionDetailsDialog::SessionDetailsDialog(string id, Session* session)
{
  setTitleBarEnabled(true);
  setCaption("Session Details");
  resize(1000, 480);
  setClosable(true);
  setResizable(true);
  Dbo::QueryModel< SessionDetailsPtr >* model = new Dbo::QueryModel<SessionDetailsPtr>();
  auto query = session->find<SessionDetails>().where("sessionInfo_sessionId = ?").bind(id).orderBy("playStarted desc");
  model->setQuery(query);
  model->addColumn("filename", "File");
  model->addColumn("playStarted", "Started");
  model->addColumn("playEnded", "Ended");
  WTableView *table = new WTableView();
  table->setItemDelegateForColumn(1, new DateTimeDelegate(model));
  table->setItemDelegateForColumn(2, new DateTimeDelegate(model));
  table->setColumn1Fixed(false);
  table->setColumnWidth(0, 300);
  table->setColumnWidth(1, 110);
  table->setColumnWidth(2, 110);
  table->setModel(model);
  contents()->addWidget(table);
  
  WTimer *timer = new WTimer(this);
  timer->setInterval(10000);
  timer->timeout().connect([model](WMouseEvent) {
    model->reload();
  });
  timer->start();
}

