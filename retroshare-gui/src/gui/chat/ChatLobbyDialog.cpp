/****************************************************************
 *
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2011, csoler
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 ****************************************************************/


#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QWidgetAction>

#include "ChatLobbyDialog.h"

#include "ChatTabWidget.h"
#include "gui/ChatLobbyWidget.h"
#include "gui/FriendsDialog.h"
#include "gui/MainWindow.h"
#include "gui/chat/ChatWidget.h"
#include "gui/common/rshtml.h"
#include "gui/common/FriendSelectionDialog.h"
#include "gui/common/RSTreeWidgetItem.h"
#include "gui/gxs/GxsIdChooser.h"
#include "gui/gxs/GxsIdDetails.h"
#include "gui/gxs/GxsIdTreeWidgetItem.h"
#include "gui/Identity/IdDialog.h"
#include "gui/msgs/MessageComposer.h"
#include "gui/settings/RsharePeerSettings.h"
#include "gui/settings/rsharesettings.h"
#include "util/HandleRichText.h"
#include "util/QtVersion.h"
#include "gui/common/AvatarDefs.h"

#include "retroshare/rsnotify.h"
#include "util/rstime.h"

#include <time.h>
#include <unistd.h>

#define COLUMN_NAME      0
#define COLUMN_ACTIVITY  1
#define COLUMN_ID        2
#define COLUMN_ICON      3
#define COLUMN_COUNT     4

#define ROLE_SORT            Qt::UserRole + 1
#define ROLE_ID             Qt::UserRole + 2

const static uint32_t timeToInactivity = 60 * 10;   // in seconds

/** Default constructor */
ChatLobbyDialog::ChatLobbyDialog(const ChatLobbyId& lid, QWidget *parent, Qt::WindowFlags flags)
        : ChatDialog(parent, flags), lobbyId(lid),
          bullet_red_128(":/app/images/statusicons/dnd.png"), bullet_grey_128(":/app/images/statusicons/bad.png"),
          bullet_green_128(":/app/images/statusicons/online.png"), bullet_yellow_128(":/app/images/statusicons/away.png"), bullet_unknown_128(":/app/images/statusicons/ask.png")
{
    /* Invoke Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    lastUpdateListTime = 0;

        //connect(ui.actionChangeNickname, SIGNAL(triggered()), this, SLOT(changeNickname()));
    connect(ui.participantsList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(participantsTreeWidgetCustomPopupMenu(QPoint)));
    connect(ui.participantsList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(participantsTreeWidgetDoubleClicked(QTreeWidgetItem*,int)));

    connect(ui.filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterChanged(QString)));

        int S = QFontMetricsF(font()).height() ;
    ui.participantsList->setIconSize(QSize(1.4*S,1.4*S));

    ui.participantsList->setColumnCount(COLUMN_COUNT);
    ui.participantsList->setColumnWidth(COLUMN_ICON, 1.7*S);
    ui.participantsList->setColumnHidden(COLUMN_ACTIVITY,true);
    ui.participantsList->setColumnHidden(COLUMN_ID,true);

    /* Set header resize modes and initial section sizes */
    QHeaderView * header = ui.participantsList->header();
    QHeaderView_setSectionResizeModeColumn(header, COLUMN_NAME, QHeaderView::Stretch);

//    muteAct = new QAction(QIcon(), tr("Mute participant"), this);
//    voteNegativeAct = new QAction(QIcon(":/icons/png/thumbs-down.png"), tr("Ban this person (Sets negative opinion)"), this);
//    voteNeutralAct = new QAction(QIcon(":/icons/png/thumbs-neutral.png"), tr("Give neutral opinion"), this);
//    votePositiveAct = new QAction(QIcon(":/icons/png/thumbs-up.png"), tr("Give positive opinion"), this);
    //distantChatAct = new QAction(QIcon(":/images/chat_24.png"), tr("Start private chat"), this);
    sendMessageAct = new QAction(QIcon(":/images/mail_new.png"), tr("Send Email"), this);
    //showInPeopleAct = new QAction(QIcon(), tr("Show author in people tab"), this);

    QActionGroup *sortgrp = new QActionGroup(this);
    actionSortByName = new QAction(QIcon(), tr("Sort by Name"), this);
    actionSortByName->setCheckable(true);
    actionSortByName->setChecked(true);
    actionSortByName->setActionGroup(sortgrp);

    actionSortByActivity = new QAction(QIcon(), tr("Sort by Activity"), this);
    actionSortByActivity->setCheckable(true);
    actionSortByActivity->setChecked(false);
    actionSortByActivity->setActionGroup(sortgrp);


//    connect(muteAct, SIGNAL(triggered()), this, SLOT(changeParticipationState()));
    //connect(distantChatAct, SIGNAL(triggered()), this, SLOT(distantChatParticipant()));
    connect(sendMessageAct, SIGNAL(triggered()), this, SLOT(sendMessage()));
//    connect(votePositiveAct, SIGNAL(triggered()), this, SLOT(voteParticipant()));
//    connect(voteNeutralAct, SIGNAL(triggered()), this, SLOT(voteParticipant()));
//    connect(voteNegativeAct, SIGNAL(triggered()), this, SLOT(voteParticipant()));
    //connect(showInPeopleAct, SIGNAL(triggered()), this, SLOT(showInPeopleTab()));

    connect(actionSortByName, SIGNAL(triggered()), this, SLOT(sortParcipants()));
    connect(actionSortByActivity, SIGNAL(triggered()), this, SLOT(sortParcipants()));

        /* Add filter actions */
    QTreeWidgetItem *headerItem = ui.participantsList->headerItem();
    QString headerText = headerItem->text(COLUMN_NAME );
    ui.filterLineEdit->setPlaceholderText("Search ");
    ui.filterLineEdit->showFilterIcon();
    //ui.filterLineEdit->addFilter(QIcon(), headerText, COLUMN_NAME , QString("%1 %2").arg(tr("Search"), headerText));

    // just empiric values
    double scaler_factor = S > 25 ? 2.4 : 1.8;
    QSize icon_size(scaler_factor * S, scaler_factor * S);

    // Add a button to invite friends.
    //
    inviteFriendsButton = new QToolButton ;
    inviteFriendsButton->setMinimumSize(icon_size);
    inviteFriendsButton->setMaximumSize(icon_size);
    inviteFriendsButton->setText(QString()) ;
    inviteFriendsButton->setAutoRaise(true) ;
    inviteFriendsButton->setToolTip(tr("Invite friends to this group"));

    mParticipantCompareRole = new RSTreeWidgetItemCompareRole;
    mParticipantCompareRole->setRole(COLUMN_ACTIVITY, ROLE_SORT);

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/invite.png")) ;
    inviteFriendsButton->setIcon(icon) ;
    inviteFriendsButton->setIconSize(icon_size);
    }

    connect(inviteFriendsButton, SIGNAL(clicked()), this , SLOT(inviteFriends()));

    getChatWidget()->addTitleBarWidget(inviteFriendsButton) ;

    RsGxsId current_id;
    rsMsgs->getIdentityForChatLobby(lobbyId, current_id);

    uint32_t idChooserFlag = IDCHOOSER_ID_REQUIRED;
    ChatLobbyInfo lobbyInfo ;
    if(rsMsgs->getChatLobbyInfo(lobbyId,lobbyInfo)) {
        if (lobbyInfo.lobby_flags & RS_CHAT_LOBBY_FLAGS_PGP_SIGNED) {
            idChooserFlag |= IDCHOOSER_NON_ANONYMOUS;
        }
    }
    ownIdChooser = new GxsIdChooser() ;
    ownIdChooser->loadIds(idChooserFlag, current_id) ;

    QWidgetAction *checkableAction = new QWidgetAction(this);
    checkableAction->setDefaultWidget(ownIdChooser);

    ui.chatWidget->addToolsAction(checkableAction);
    //getChatWidget()->addChatBarWidget(ownIdChooser);
    connect(ui.chatWidget, SIGNAL(textBrowserAskContextMenu(QMenu*,QString,QPoint)), this, SLOT(textBrowserAskContextMenu(QMenu*,QString,QPoint)));



    connect(ownIdChooser,SIGNAL(currentIndexChanged(int)),this,SLOT(changeNickname())) ;

    unsubscribeButton = new QToolButton;
    unsubscribeButton->setMinimumSize(icon_size);
    unsubscribeButton->setMaximumSize(icon_size);
    unsubscribeButton->setText(QString()) ;
    unsubscribeButton->setAutoRaise(true) ;
    unsubscribeButton->setToolTip(tr("Leave this group chat"));

    {
    QIcon icon ;
    icon.addPixmap(QPixmap(":/icons/png/leave.png")) ;
    unsubscribeButton->setIcon(icon) ;
    unsubscribeButton->setIconSize(icon_size);
    }

    /* Initialize splitter */
    ui.splitter->setStretchFactor(0, 1);
    ui.splitter->setStretchFactor(1, 0);
    ui.splitter->setCollapsible(0, false);
    ui.splitter->setCollapsible(1, false);

    connect(unsubscribeButton, SIGNAL(clicked()), this , SLOT(leaveLobby()));

    getChatWidget()->addTitleBarWidget(unsubscribeButton) ;
}

void ChatLobbyDialog::leaveLobby()
{
    emit lobbyLeave(id()) ;
}
void ChatLobbyDialog::inviteFriends()
{
    std::cerr << "Inviting friends" << std::endl;

    std::set<RsPeerId> ids = FriendSelectionDialog::selectFriends_SSL(NULL,tr("Invite friends"),tr("Select friends to invite:")) ;

    std::cerr << "Inviting these friends:" << std::endl;

    if (!mChatId.isLobbyId())
        return ;

    for(std::set<RsPeerId>::const_iterator it(ids.begin());it!=ids.end();++it)
    {
        std::cerr << "    " << *it  << std::endl;

        rsMsgs->invitePeerToLobby(mChatId.toLobbyId(),*it) ;
    }
}

void ChatLobbyDialog::participantsTreeWidgetCustomPopupMenu(QPoint)
{
    QList<QTreeWidgetItem*> selectedItems = ui.participantsList->selectedItems();
    QList<RsGxsId> idList;
    QList<QTreeWidgetItem*>::iterator item;
    for (item = selectedItems.begin(); item != selectedItems.end(); ++item)
    {
        RsGxsId gxs_id ;
        dynamic_cast<GxsIdRSTreeWidgetItem*>(*item)->getId(gxs_id) ;
        idList.append(gxs_id);
    }

    QMenu contextMnu(this);
    contextMnu.addAction(actionSortByActivity);
    contextMnu.addAction(actionSortByName);

    contextMnu.addSeparator();

    initParticipantsContextMenu(&contextMnu, idList);

    contextMnu.exec(QCursor::pos());
}

void ChatLobbyDialog::textBrowserAskContextMenu(QMenu* contextMnu, QString anchorForPosition, const QPoint /*point*/)
{
    if (anchorForPosition.startsWith(PERSONID)) {
        QString strId = anchorForPosition.replace(PERSONID, "");
        if (strId.contains(" "))
            strId.truncate(strId.indexOf(" "));

        contextMnu->addSeparator();

        QList<RsGxsId> idList;
        idList.append(RsGxsId(strId.toStdString()));
        initParticipantsContextMenu(contextMnu, idList);
    }
}

void ChatLobbyDialog::initParticipantsContextMenu(QMenu *contextMnu, QList<RsGxsId> idList)
{
    if (!contextMnu)
        return;
    if (idList.isEmpty())
        return;

    //contextMnu->addAction(distantChatAct);
    contextMnu->addAction(sendMessageAct);
//    contextMnu->addSeparator();
//    contextMnu->addAction(muteAct);
//    contextMnu->addAction(votePositiveAct);
//    contextMnu->addAction(voteNeutralAct);
//    contextMnu->addAction(voteNegativeAct);
    //contextMnu->addAction(showInPeopleAct);

    //distantChatAct->setEnabled(false);
    sendMessageAct->setEnabled(true);
//    muteAct->setEnabled(false);
//    muteAct->setCheckable(true);
//    muteAct->setChecked(false);
//    votePositiveAct->setEnabled(false);
//    voteNeutralAct->setEnabled(false);
//    voteNegativeAct->setEnabled(false);
    //showInPeopleAct->setEnabled(idList.count() == 1);

    //distantChatAct->setData(QVariant::fromValue(idList));
    sendMessageAct->setData(QVariant::fromValue(idList));
//    muteAct->setData(QVariant::fromValue(idList));
//    votePositiveAct->setData(QVariant::fromValue(idList));
//    voteNeutralAct->setData(QVariant::fromValue(idList));
//    voteNegativeAct->setData(QVariant::fromValue(idList));
    //showInPeopleAct->setData(QVariant::fromValue(idList));

//    RsGxsId gxsid = idList.at(0);

//    if(!gxsid.isNull() && !rsIdentity->isOwnId(gxsid))
//    {
//        //distantChatAct->setEnabled(true);
//        votePositiveAct->setEnabled(rsReputations->overallReputationLevel(gxsid) != RsReputations::REPUTATION_LOCALLY_POSITIVE);
//        voteNeutralAct->setEnabled((rsReputations->overallReputationLevel(gxsid) == RsReputations::REPUTATION_LOCALLY_POSITIVE) || (rsReputations->overallReputationLevel(gxsid) == RsReputations::REPUTATION_LOCALLY_NEGATIVE) );
//        voteNegativeAct->setEnabled(rsReputations->overallReputationLevel(gxsid) != RsReputations::REPUTATION_LOCALLY_NEGATIVE);
//        muteAct->setEnabled(true);
//        muteAct->setChecked(isParticipantMuted(gxsid));
//    }
}

void ChatLobbyDialog::voteParticipant()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

//    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();

//    RsReputations::Opinion op = RsReputations::OPINION_NEUTRAL ;
//    if (act == votePositiveAct)
//        op = RsReputations::OPINION_POSITIVE;
//    if (act == voteNegativeAct)
//        op = RsReputations::OPINION_NEGATIVE;

//    for (QList<RsGxsId>::iterator item = idList.begin(); item != idList.end(); ++item)
//    {
//        rsReputations->setOwnOpinion(*item, op);
//        std::cerr << "Giving opinion to GXS id " << *item << " to " << op << std::endl;
//    }

    updateParticipantsList();
}

void ChatLobbyDialog::showInPeopleTab()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();
    if (idList.count() != 1)
        return;

    RsGxsId nickname = idList.at(0);

    IdDialog *idDialog = dynamic_cast<IdDialog*>(MainWindow::getPage(MainWindow::People));
    if (!idDialog)
        return ;
    MainWindow::showWindow(MainWindow::People);

    idDialog->navigate(nickname);
}

void ChatLobbyDialog::init(const ChatId &/*id*/, const QString &/*title*/)
{
    ChatLobbyInfo linfo ;

    QString title;

    if(rsMsgs->getChatLobbyInfo(lobbyId,linfo))
    {
        title = QString::fromUtf8(linfo.lobby_name.c_str());

        QString msg = tr("Welcome to group chat: %1").arg(RsHtml::plainText(linfo.lobby_name));
        _lobby_name = QString::fromUtf8(linfo.lobby_name.c_str()) ;
        if (!linfo.lobby_topic.empty()) {
            msg += "\n" + tr("Topic: %1").arg(RsHtml::plainText(linfo.lobby_topic));
        }
        ui.chatWidget->setWelcomeMessage(msg);
    }

    ChatDialog::init(ChatId(lobbyId), title);

    RsGxsId gxs_id;
    rsMsgs->getIdentityForChatLobby(lobbyId, gxs_id);

    RsIdentityDetails details ;

    // This lets the backend some time to load our own identity in cache.
    // It will only loop for at most 1 second the first time.

    for(int i=0;i<3;++i)
        if(rsIdentity->getIdDetails(gxs_id,details))
            break ;
        else
            rstime::rs_usleep(1000*300) ;

    ui.chatWidget->setName(QString::fromUtf8(details.mNickname.c_str()));
    //ui.chatWidget->addToolsAction(ui.actionChangeNickname);
    ui.chatWidget->setDefaultExtraFileFlags(RS_FILE_REQ_ANONYMOUS_ROUTING);

    lastUpdateListTime = 0;

    // add to window

    ChatLobbyWidget *chatLobbyPage = dynamic_cast<ChatLobbyWidget*>(MainWindow::getPage(MainWindow::ChatLobby));
    if (chatLobbyPage) {
        chatLobbyPage->addChatPage(this) ;
    }

    /** List of muted Participants */
    mutedParticipants.clear() ;

    //try to update the member status on member list
    updateParticipantsList();

    // load settings
    processSettings(true);
}

/** Destructor. */
ChatLobbyDialog::~ChatLobbyDialog()
{
    // announce leaving of lobby

    //unseenp2p - no need to leave group (unsubscribeChatLobby = leave group)
    // check that the lobby still exists.
//    if (mChatId.isLobbyId()) {
//        rsMsgs->unsubscribeChatLobby(mChatId.toLobbyId());
//	}

    // save settings
    processSettings(false);
}

ChatWidget *ChatLobbyDialog::getChatWidget()
{
    return ui.chatWidget;
}

bool ChatLobbyDialog::notifyBlink()
{
    return (Settings->getChatLobbyFlags() & RS_CHATLOBBY_BLINK);
}

void ChatLobbyDialog::processSettings(bool load)
{
    Settings->beginGroup(QString("ChatLobbyDialog"));

    if (load) {
        // load settings

        // state of splitter
        ui.splitter->restoreState(Settings->value("splitter").toByteArray());

        // load sorting
        actionSortByActivity->setChecked(Settings->value("sortbyActivity", QVariant(false)).toBool());
        actionSortByName->setChecked(Settings->value("sortbyName", QVariant(true)).toBool());

        //try to open the last chat window from here
        ChatLobbyWidget *chatLobbyPage = dynamic_cast<ChatLobbyWidget*>(MainWindow::getPage(MainWindow::ChatLobby));
        if (chatLobbyPage) {
            chatLobbyPage->openLastChatWindow();
        }
    } else {
        // save settings

        // state of splitter
        Settings->setValue("splitter", ui.splitter->saveState());

        //save sorting
        Settings->setValue("sortbyActivity", actionSortByActivity->isChecked());
        Settings->setValue("sortbyName", actionSortByName->isChecked());
    }

    Settings->endGroup();
}

/**
 * Change your Nickname
 *
 * - send a Message to all Members => later: send hidden message to clients, so they can actualize there mutedParticipants list
 */
void ChatLobbyDialog::setIdentity(const RsGxsId& gxs_id)
{
    rsMsgs->setIdentityForChatLobby(lobbyId, gxs_id) ;

    // get new nick name
    RsGxsId newid;

    if (rsMsgs->getIdentityForChatLobby(lobbyId, newid))
    {
        RsIdentityDetails details ;
        rsIdentity->getIdDetails(gxs_id,details) ;

        ui.chatWidget->setName(QString::fromUtf8(details.mNickname.c_str()));
    }
}

/**
 * Dialog: Change your Nickname in the ChatLobby
 */
void ChatLobbyDialog::changeNickname()
{
    RsGxsId current_id;
    rsMsgs->getIdentityForChatLobby(lobbyId, current_id);

    RsGxsId new_id ;
    ownIdChooser->getChosenId(new_id) ;

    if(!new_id.isNull() && new_id != current_id)
        setIdentity(new_id);
}

/**
 * We get a new Message from a chat participant
 *
 * - Ignore Messages from muted chat participants
 */
void ChatLobbyDialog::addChatMsg(const ChatMessage& msg)
{
    QDateTime sendTime = QDateTime::fromTime_t(msg.sendTime);
    QDateTime recvTime = QDateTime::fromTime_t(msg.recvTime);
    QString message = QString::fromUtf8(msg.msg.c_str());
    RsGxsId gxs_id = msg.lobby_peer_gxs_id ;

    if(!isParticipantMuted(gxs_id))
    {
        // We could change addChatMsg to display the peers icon, passing a ChatId

        RsIdentityDetails details ;

        QString name ;
        if(rsIdentity->getIdDetails(gxs_id,details))
            name = QString::fromUtf8(details.mNickname.c_str()) ;
        else
            name = QString::fromUtf8(msg.peer_alternate_nickname.c_str()) + " (" + QString::fromStdString(gxs_id.toStdString()) + ")" ;

        ui.chatWidget->addChatMsg(msg.incoming, name, gxs_id, sendTime, recvTime, message, ChatWidget::MSGTYPE_NORMAL);
        emit messageReceived(msg.incoming, id(), sendTime, name, message) ;

        // This is a trick to translate HTML into text.
        QTextEdit editor;
        editor.setHtml(message);
        QString notifyMsg = name + ": " + editor.toPlainText();

        if(msg.incoming)
        {
            if(notifyMsg.length() > 30)
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + _lobby_name, notifyMsg.left(30) + QString("..."));
            else
                MainWindow::displayLobbySystrayMsg(tr("Group chat") + ": " + _lobby_name, notifyMsg);
        }

    }

    // also update peer list.

    time_t now = time(NULL);

   QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(QString::fromStdString(gxs_id.toStdString()),Qt::MatchExactly,COLUMN_ID);
    if (qlFoundParticipants.count()!=0) qlFoundParticipants.at(0)->setText(COLUMN_ACTIVITY,QString::number(now));

    if (now > lastUpdateListTime) {
        lastUpdateListTime = now;
        updateParticipantsList();
    }
}

/**
 * Regenerate the QTreeWidget participant list of a Chat Lobby
 *
 * Show yellow icon for muted Participants
 */
void ChatLobbyDialog::updateParticipantsList()
{
    ChatLobbyInfo linfo;

    bool hasNewMemberJoin = false;
    if(rsMsgs->getChatLobbyInfo(lobbyId,linfo))
    {

        //rsMsgs->locked_printDebugInfo();
        rstime_t now = time(NULL) ;
//        std::cerr << "   Participating friends: " << std::endl;
//        std::cerr << "   groupchat name: " << linfo.lobby_name << std::endl;
//        std::cerr << "   Participating nick names (rsgxsId): " << std::endl;

//        for(std::map<RsGxsId,rstime_t>::const_iterator it2(linfo.gxs_ids.begin());it2!=linfo.gxs_ids.end();++it2)
//            std::cerr << "       " << it2->first << ": " << now - it2->second << " secs ago" << std::endl;

        ChatLobbyInfo cliInfo=linfo;
        QList<QTreeWidgetItem*>  qlOldParticipants=ui.participantsList->findItems("*",Qt::MatchWildcard,COLUMN_ID);

        foreach(QTreeWidgetItem *qtwiCur,qlOldParticipants)
            if(cliInfo.gxs_ids.find(RsGxsId((*qtwiCur).text(COLUMN_ID).toStdString())) == cliInfo.gxs_ids.end())
            {
                //Old Participant go out, remove it
                int index = ui.participantsList->indexOfTopLevelItem(qtwiCur);
                delete ui.participantsList->takeTopLevelItem(index);
            }


        for (auto it2(linfo.gxs_ids.begin()); it2 != linfo.gxs_ids.end(); ++it2)
        {
            QString participant = QString::fromUtf8( (it2->first).toStdString().c_str() );

            QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(participant,Qt::MatchExactly,COLUMN_ID);
            GxsIdRSTreeWidgetItem *widgetitem;

            if (qlFoundParticipants.count()==0)
            {
                // TE: Add Wigdet to participantsList with Checkbox, to mute Participant

                widgetitem = new GxsIdRSTreeWidgetItem(mParticipantCompareRole,GxsIdDetails::ICON_TYPE_AVATAR);
                widgetitem->setId(it2->first,COLUMN_NAME, true) ;
                //widgetitem->setText(COLUMN_NAME, participant);
                // set activity time to the oast so that the peer is marked as inactive
                widgetitem->setText(COLUMN_ACTIVITY,QString::number(time(NULL) - timeToInactivity));
                widgetitem->setText(COLUMN_ID,QString::fromStdString(it2->first.toStdString()));

                ui.participantsList->addTopLevelItem(widgetitem);
                hasNewMemberJoin = true;
            }
            else
                widgetitem = dynamic_cast<GxsIdRSTreeWidgetItem*>(qlFoundParticipants.at(0));

            if (isParticipantMuted(it2->first)) {
                widgetitem->setTextColor(COLUMN_NAME,QColor(255,0,0));
            } else {
                widgetitem->setTextColor(COLUMN_NAME,ui.participantsList->palette().color(QPalette::Active, QPalette::Text));
            }

            //try to update the avatar
            widgetitem->forceUpdate();

            time_t tLastAct=widgetitem->text(COLUMN_ACTIVITY).toInt();
            time_t now = time(NULL);

            widgetitem->setSizeHint(COLUMN_ICON, QSize(20,20));

            //Change the member status using ssl connection here
            RsIdentityDetails details;
            RsPeerId sslId;
            if (rsIdentity->getIdDetails(it2->first, details ))
            {
                RsPeerDetails detail;
                if (rsPeers->getGPGDetails(details.mPgpId, detail))
                {
                    std::list<RsPeerId> sslIds;
                    rsPeers->getAssociatedSSLIds(detail.gpg_id, sslIds);
                    if (sslIds.size() >= 1) {
                        sslId = sslIds.front();
                    }
                }
            }

            if (!sslId.isNull())
            {
                StatusInfo statusContactInfo;
                rsStatus->getStatus(sslId,statusContactInfo);
                switch (statusContactInfo.status)
                {
                case RS_STATUS_OFFLINE:
                    widgetitem->setIcon(COLUMN_ICON, bullet_grey_128 );
                    break;
                case RS_STATUS_INACTIVE:
                    widgetitem->setIcon(COLUMN_ICON, bullet_yellow_128 );
                    break;
                case RS_STATUS_ONLINE:
                    widgetitem->setIcon(COLUMN_ICON, bullet_green_128);
                    break;
                case RS_STATUS_AWAY:
                    widgetitem->setIcon(COLUMN_ICON, bullet_yellow_128);
                    break;
                case RS_STATUS_BUSY:
                    widgetitem->setIcon(COLUMN_ICON, bullet_red_128);
                    break;
                }
            }
            else
            {
                widgetitem->setIcon(COLUMN_ICON, bullet_unknown_128 );
            }

//            if(isParticipantMuted(it2->first))
//                widgetitem->setIcon(COLUMN_ICON, bullet_red_128);
//            else if (tLastAct + timeToInactivity < now)
//                widgetitem->setIcon(COLUMN_ICON, bullet_grey_128 );
//            else
//                widgetitem->setIcon(COLUMN_ICON, bullet_green_128);

            RsGxsId gxs_id;
            rsMsgs->getIdentityForChatLobby(lobbyId, gxs_id);

            if (RsGxsId(participant.toStdString()) == gxs_id)
                widgetitem->setIcon(COLUMN_ICON, bullet_green_128);

            widgetitem->updateBannedState();

            QTime qtLastAct=QTime(0,0,0).addSecs(now-tLastAct);
//            widgetitem->setToolTip(COLUMN_ICON,tr("Right click to mute/unmute participants<br/>Double click to address this person<br/>")
//                                   +tr("This participant is not active since:")
//                                   +qtLastAct.toString()
//                                   +tr(" seconds")
//                                   );
        }

    }
    if (hasNewMemberJoin)
    {
        rsMsgs->saveGroupChatInfo();
    }
    ui.participantsList->setSortingEnabled(true);
    sortParcipants();
    filterIds();
}

/**
 * Called when a Participant get Clicked / Changed
 *
 * Check if the Checkbox altered and Mute User
 */
void ChatLobbyDialog::changeParticipationState()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();

    for (QList<RsGxsId>::iterator item = idList.begin(); item != idList.end(); ++item)
    {
        std::cerr << "check Partipation status for '" << *item << std::endl;
        if (act->isChecked()) {
            muteParticipant(*item);
        } else {
            unMuteParticipant(*item);
        }
    }

    updateParticipantsList();
}

void ChatLobbyDialog::participantsTreeWidgetDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item) {
        return;
    }

    if(column == COLUMN_NAME)
    {
        getChatWidget()->pasteText("@" + RsHtml::plainText(item->text(COLUMN_NAME))) ;
        return ;
    }

//	if (column == COLUMN_ICON) {
//		return;
//	}
//
//	QString nickname = item->text(COLUMN_NAME);
//	if (isParticipantMuted(nickname)) {
//		unMuteParticipant(nickname);
//	} else {
//		muteParticipant(nickname);
//	}
//
//	mutedParticipants->removeDuplicates();
//
//	updateParticipantsList();
}

void ChatLobbyDialog::distantChatParticipant()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    std::cerr << " initiating distant chat" << std::endl;

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();
    if (idList.count() != 1)
        return;

    RsGxsId gxs_id = idList.at(0);
    if (gxs_id.isNull())
        return;

    RsGxsId own_id;
    rsMsgs->getIdentityForChatLobby(lobbyId, own_id);

    DistantChatPeerId tunnel_id;
    uint32_t error_code ;

    if(! rsMsgs->initiateDistantChatConnexion(gxs_id,own_id,tunnel_id,error_code))
    {
        QString error_str ;
        switch(error_code)
        {
            case RS_DISTANT_CHAT_ERROR_DECRYPTION_FAILED   : error_str = tr("Decryption failed.") ; break ;
            case RS_DISTANT_CHAT_ERROR_SIGNATURE_MISMATCH  : error_str = tr("Signature mismatch") ; break ;
            case RS_DISTANT_CHAT_ERROR_UNKNOWN_KEY         : error_str = tr("Unknown key") ; break ;
            case RS_DISTANT_CHAT_ERROR_UNKNOWN_HASH        : error_str = tr("Unknown hash") ; break ;
            default:
                error_str = tr("Unknown error.") ;
        }
        QMessageBox::warning(NULL,tr("Cannot start distant chat"),tr("Distant chat cannot be initiated:")+" "+error_str
                             +QString::number(error_code)) ;
    }
}

void ChatLobbyDialog::sendMessage()
{
    QAction *act = dynamic_cast<QAction*>(sender()) ;
    if(!act)
    {
        std::cerr << "No sender! Some bug in the code." << std::endl;
        return ;
    }

    QList<RsGxsId> idList = act->data().value<QList<RsGxsId>>();

    MessageComposer *nMsgDialog = MessageComposer::newMsg();
    if (nMsgDialog == NULL)
        return;

    for (QList<RsGxsId>::iterator item = idList.begin(); item != idList.end(); ++item)
        nMsgDialog->addRecipient(MessageComposer::TO,  *item);

    nMsgDialog->show();
    nMsgDialog->activateWindow();

    /* window will destroy itself! */
}


void ChatLobbyDialog::muteParticipant(const RsGxsId& nickname)
{
    std::cerr << " Mute " << std::endl;

    RsGxsId gxs_id;
    rsMsgs->getIdentityForChatLobby(lobbyId, gxs_id);

    if (gxs_id!=nickname)
        mutedParticipants.insert(nickname);
}

void ChatLobbyDialog::unMuteParticipant(const RsGxsId& id)
{
    std::cerr << " UnMute " << std::endl;
    mutedParticipants.erase(id);
}

/**
 * Is this nickName already known in the lobby
 */
bool ChatLobbyDialog::isNicknameInLobby(const RsGxsId& nickname)
{
    ChatLobbyInfo clinfo;

    if(! rsMsgs->getChatLobbyInfo(lobbyId,clinfo))
        return false ;

    return clinfo.gxs_ids.find(nickname) != clinfo.gxs_ids.end() ;
}

/**
 * Should Messages from this Nickname be muted?
 *
 * At the moment it is not possible to 100% know which peer sendet the message, and only
 * the nickname is available. So this couldn't work for 100%. So, for example,  if a peer
 * change his name to the name of a other peer, we couldn't block him. A real implementation
 * will be possible if we transfer a temporary Session ID from the sending Retroshare client
 * version 0.6
 *
 * @param QString nickname to check
 */
bool ChatLobbyDialog::isParticipantMuted(const RsGxsId& participant)
{
    // nickname in Mute list
    return mutedParticipants.find(participant) != mutedParticipants.end();
}

QString ChatLobbyDialog::getParticipantName(const RsGxsId& gxs_id) const
{
    RsIdentityDetails details ;

    QString name ;
    if(rsIdentity->getIdDetails(gxs_id,details))
        name = QString::fromUtf8(details.mNickname.c_str()) ;
    else
        name = QString::fromUtf8("[Unknown] (") + QString::fromStdString(gxs_id.toStdString()) + ")" ;

    return name ;
}


void ChatLobbyDialog::displayLobbyEvent(int event_type, const RsGxsId& gxs_id, const QString& str)
{
    RsGxsId qsParticipant;

    QString name= getParticipantName(gxs_id) ;

    switch (event_type)
    {
    case RS_CHAT_LOBBY_EVENT_PEER_LEFT:
        qsParticipant=gxs_id;
        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 has left the room.").arg(RsHtml::plainText(name)), ChatWidget::MSGTYPE_SYSTEM);
        emit peerLeft(id()) ;
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_JOINED:
        qsParticipant=gxs_id;
        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(), QDateTime::currentDateTime(), tr("%1 joined the room.").arg(RsHtml::plainText(name)), ChatWidget::MSGTYPE_SYSTEM);
        emit peerJoined(id()) ;
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_STATUS:
    {

        qsParticipant=gxs_id;

        ui.chatWidget->updateStatusString(RsHtml::plainText(name) + " %1", RsHtml::plainText(str));

        if (!isParticipantMuted(gxs_id))
            emit typingEventReceived(id()) ;

    }
        break;
    case RS_CHAT_LOBBY_EVENT_PEER_CHANGE_NICKNAME:
    {
        qsParticipant=gxs_id;

        QString newname= getParticipantName(RsGxsId(str.toStdString())) ;

        ui.chatWidget->addChatMsg(true, tr("Chat room management"), QDateTime::currentDateTime(),
                                  QDateTime::currentDateTime(),
                                  tr("%1 changed his name to: %2").arg(RsHtml::plainText(name)).arg(RsHtml::plainText(newname)),
                                  ChatWidget::MSGTYPE_SYSTEM);

        // TODO if a user was muted and changed his name, update mute list, but only, when the muted peer, dont change his name to a other peer in your chat lobby
        if (isParticipantMuted(gxs_id))
            muteParticipant(RsGxsId(str.toStdString())) ;
    }
        break;
    case RS_CHAT_LOBBY_EVENT_KEEP_ALIVE:
        //std::cerr << "Received keep alive packet from " << nickname.toStdString() << " in chat room " << getPeerId() << std::endl;
        break;
    default:
        std::cerr << "ChatLobbyDialog::displayLobbyEvent() Unhandled chat room event type " << event_type << std::endl;
    }

    if (!qsParticipant.isNull())
    {
        QList<QTreeWidgetItem*>  qlFoundParticipants=ui.participantsList->findItems(QString::fromStdString(qsParticipant.toStdString()),Qt::MatchExactly,COLUMN_ID);

        if (qlFoundParticipants.count()!=0)
        qlFoundParticipants.at(0)->setText(COLUMN_ACTIVITY,QString::number(time(NULL)));
    }

    updateParticipantsList() ;
}

bool ChatLobbyDialog::canClose()
{
    // check that the lobby still exists.
    /* TODO
    ChatLobbyId lid;
    if (!rsMsgs->isLobbyId(getPeerId(), lid)) {
        return true;
    }
    */

    if (QMessageBox::Yes == QMessageBox::question(this, tr("Unsubscribe from chat room"), tr("Do you want to unsubscribe to this chat room?"), QMessageBox::Yes | QMessageBox::No)) {
        return true;
    }

    return false;
}

void ChatLobbyDialog::showDialog(uint chatflags)
{
    if (chatflags & RS_CHAT_FOCUS)
    {
        MainWindow::showWindow(MainWindow::ChatLobby);
        dynamic_cast<ChatLobbyWidget*>(MainWindow::getPage(MainWindow::ChatLobby))->setCurrentChatPage(this) ;
    }
}

void ChatLobbyDialog::sortParcipants()
{

    if (actionSortByActivity->isChecked()) {
        ui.participantsList->sortItems(COLUMN_ACTIVITY, Qt::DescendingOrder);
    } else if (actionSortByName->isChecked()) {
        ui.participantsList->sortItems(COLUMN_NAME, Qt::AscendingOrder);
    }

}

void ChatLobbyDialog::filterChanged(const QString& /*text*/)
{
    filterIds();
}

void ChatLobbyDialog::filterIds()
{
    int filterColumn = ui.filterLineEdit->currentFilter();
    QString text = ui.filterLineEdit->text();

    ui.participantsList->filterItems(filterColumn, text);
}
