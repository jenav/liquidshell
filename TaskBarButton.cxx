#include <TaskBarButton.hxx>

#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionButton>
#include <QPainter>
#include <QX11Info>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDrag>
#include <QMimeData>
#include <QGuiApplication>
#include <QStyleHints>

#include <KSqueezedTextLabel>
#include <KColorScheme>
#include <KLocalizedString>

#include <netwm.h>

//--------------------------------------------------------------------------------

TaskBarButton::TaskBarButton(WId theWid)
  : wid(theWid)
{
  setAutoFillBackground(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setAcceptDrops(true);
  dragDropTimer.setSingleShot(true);
  dragDropTimer.setInterval(1000);
  connect(&dragDropTimer, &QTimer::timeout,
          [this]() { KWindowSystem::raiseWindow(wid); KWindowSystem::forceActiveWindow(wid); });

  QHBoxLayout *hbox = new QHBoxLayout(this);
  hbox->setContentsMargins(QMargins(4, 2, 4, 4));

  iconLabel = new QLabel;
  iconLabel->setScaledContents(true);
  iconLabel->setFixedSize(32, 32);
  iconLabel->setContextMenuPolicy(Qt::PreventContextMenu);
  hbox->addWidget(iconLabel);

  textLabel = new KSqueezedTextLabel;
  textLabel->setTextElideMode(Qt::ElideRight);
  textLabel->setContextMenuPolicy(Qt::PreventContextMenu);
  hbox->addWidget(textLabel);

  fill();
  setBackground();

  connect(KWindowSystem::self(), SIGNAL(windowChanged(WId, NET::Properties, NET::Properties2)),
          this, SLOT(windowChanged(WId, NET::Properties, NET::Properties2)));

  connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged,
          this, &TaskBarButton::setBackground);
}

//--------------------------------------------------------------------------------

void TaskBarButton::fill()
{
  KWindowInfo win(wid, NET::WMName | NET::WMIcon);
  iconLabel->setPixmap(KWindowSystem::icon(wid, 32, 32, true));
  textLabel->setText(win.name());
  setToolTip(win.name());
}

//--------------------------------------------------------------------------------

void TaskBarButton::mousePressEvent(QMouseEvent *event)
{
  if ( event->button() == Qt::LeftButton )
  {
    if ( wid == KWindowSystem::activeWindow() )
      KWindowSystem::minimizeWindow(wid);
    else
      KWindowSystem::forceActiveWindow(wid);

    //dragStartTimer.start();
    dragStartPos = event->pos();
    event->accept();
  }
  else if ( event->button() == Qt::RightButton )
  {
    // context menu to close window etc.
    QScopedPointer<QMenu> menu(new QMenu(this));

    if ( KWindowSystem::numberOfDesktops() > 1 )
    {
      QMenu *desktops = menu->addMenu(i18n("Move To Desktop"));
      desktops->addAction(i18n("All Desktops"), [this]() { KWindowSystem::setOnAllDesktops(wid, true); });
      desktops->addSeparator();

      for (int i = 1; i <= KWindowSystem::numberOfDesktops(); i++)
        desktops->addAction(KWindowSystem::desktopName(i), [this, i]() { KWindowSystem::setOnDesktop(wid, i); });
    }

    menu->addAction(QIcon::fromTheme("window-close"), i18n("Close"),
                    [this]()
                    {
                      NETRootInfo ri(QX11Info::connection(), NET::CloseWindow);
                      ri.closeWindowRequest(wid);
                    }
                   );

    menu->exec(event->globalPos());
  }
}

//--------------------------------------------------------------------------------

void TaskBarButton::mouseReleaseEvent(QMouseEvent *event)
{
  event->accept();
  //dragStartTimer.invalidate();
}

//--------------------------------------------------------------------------------

void TaskBarButton::mouseMoveEvent(QMouseEvent *event)
{
  event->accept();

  /*
  if ( !rect().contains(event->pos()) )
  {
    event->ignore();
    dragStartTimer.invalidate();
    return;
  }
  */

  if ( /*dragStartTimer.isValid() && (dragStartTimer.elapsed() > QGuiApplication::styleHints()->startDragTime()) &&*/
       ((event->pos() - dragStartPos).manhattanLength() > QGuiApplication::styleHints()->startDragDistance()) )
  {
    QDrag *drag = new QDrag(parentWidget());
    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-winId", QByteArray::number(static_cast<int>(wid)));
    drag->setMimeData(mimeData);
    drag->setPixmap(*(iconLabel->pixmap()));
    drag->exec();
  }
}

//--------------------------------------------------------------------------------

void TaskBarButton::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);

  QStyleOptionButton option;
  initStyleOption(&option);

  style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);
}

//--------------------------------------------------------------------------------

void TaskBarButton::windowChanged(WId id, NET::Properties props, NET::Properties2 props2)
{
  Q_UNUSED(id)
  Q_UNUSED(props2)

  //qDebug() << textLabel->text() << props << (props & (NET::WMVisibleName | NET::WMState |  NET::WMName));
  //qDebug() << this << id << props << "me" << (wid == id);
  //if ( (id != wid) || (props == 0) )
    //return;

  if ( props & (NET::WMState | NET::ActiveWindow) )
    setBackground();

  // WMVisibleName alone is not enough. WMName needed
  if ( (wid == id) && (props & (NET::WMIcon | NET::WMName)) )
    fill();
}

//--------------------------------------------------------------------------------

void TaskBarButton::setBackground()
{
  KColorScheme scheme(QPalette::Active, KColorScheme::Window);
  QPalette pal = palette();

  KWindowInfo win(wid, NET::WMState);

  if ( win.state() & NET::Hidden )
    pal.setBrush(foregroundRole(), scheme.foreground(KColorScheme::InactiveText));
  else
    pal.setBrush(foregroundRole(), scheme.foreground(KColorScheme::NormalText));

  QBrush brush;

  if ( win.state() & NET::DemandsAttention )
    brush = scheme.background(KColorScheme::ActiveBackground);
  else if ( wid == KWindowSystem::activeWindow() )
    brush = scheme.shade(KColorScheme::MidShade);
  else
    brush = scheme.background();

  pal.setBrush(backgroundRole(), brush);
  setPalette(pal);
}

//--------------------------------------------------------------------------------

void TaskBarButton::dragEnterEvent(QDragEnterEvent *event)
{
  event->accept();
  dragDropTimer.start();
}

//--------------------------------------------------------------------------------

void TaskBarButton::dragLeaveEvent(QDragLeaveEvent *event)
{
  event->accept();
  dragDropTimer.stop();
}

//--------------------------------------------------------------------------------

void TaskBarButton::dropEvent(QDropEvent *event)
{
  event->accept();
  dragDropTimer.stop();
}

//--------------------------------------------------------------------------------