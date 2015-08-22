﻿#include "stulogin.h"
#include <QNetworkRequest>
#include <QMessageBox>
#include <QTextCodec>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
QString STULogin::LOGIN_REQUEST_ADDR = "http://192.168.31.4:8080/?url=" ;
QString STULogin::LOGOUT_REQUEST_ADDR = "http://192.168.31.4:8080/?status=ok&url=";
QString STULogin::USERNAME_INPUT = "AuthenticateUser";
QString STULogin::PASSWD_INPUT = "AuthenticatePassword";
QString STULogin::LOGIN_INPUT = "Submit";
QString STULogin::LOGOUT_INPUT = "Logout";
QString STULogin::USERNAME_POSITION = "<td width=\"262\" class=\"text3\">";
QString STULogin::USEDBYTE_POSITION = "<td class=\"text3\" id=\"ub\">";
QString STULogin::TOTALBYTE_POSITION = "<td class=\"text3\" id=\"tb\">";
QString STULogin::POSITION_END = "</td>";
QString STULogin::INVALID = "Invalid";
QString STULogin::CONNECTED = "Used bytes";
int STULogin::CONVERT_RATE = 1024 * 1024;
int STULogin::MAX_ERROR_COUNT = 10;

STULogin::STULogin(QObject *parent): QObject(parent), is_connected(false), autoChange(false), stopChaneing(false), logining(false),
thresholdValue(1), ptr_account(-1){
    manager = new QNetworkAccessManager(this);
    codec = QTextCodec::codecForName("utf-8");
    requestAddr = new QUrl(LOGIN_REQUEST_ADDR);  // default address
    delayTimer = new QTimer;

}

void STULogin::logout(){
    logining = false;   // need this flag becuase the school server sucks
    wrongCount = 0;
    requestAddr->setUrl(LOGOUT_REQUEST_ADDR);
    QUrl params;
    params.addQueryItem(LOGOUT_INPUT,"");
    QByteArray postData = params.encodedQuery();
    QNetworkRequest request(*requestAddr);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    QEventLoop eventLoop;   // for synchronization
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            &eventLoop, SLOT(quit()));
    QNetworkReply *reply = manager->post(request, postData);
    eventLoop.exec();
    // means that the reply is ready to be read
    handleReply(reply);
    processStates(replyData);
    if (is_connected && autoChange && left <= thresholdValue)
        logout();   // until logout
    emit stateChanged(is_connected,user,used,total,left);
}

void STULogin::login(const QString &user, const QString &passwd){

//    qDebug() << "now login " + user + "\t" + passwd;

    logining = true;  // QtNetWorkAccessManager works asynchronously
    this->user_copy = user;
//    qDebug() << this->user;
    this->passwd = passwd;  // record the user info for try again untill MAX_COUNT or login
    if (is_connected){
        wrongCount = 0;
        return;
    }
    requestAddr->setUrl(LOGIN_REQUEST_ADDR);
    QUrl params;
    params.addQueryItem(USERNAME_INPUT,user);
    params.addQueryItem(PASSWD_INPUT,passwd);
    params.addQueryItem(LOGIN_INPUT,"");

    QByteArray postData = params.encodedQuery();
    QNetworkRequest request(*requestAddr);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");
    QEventLoop eventLoop;   // for synchronization
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            &eventLoop, SLOT(quit()));
    delayForSomeTime(600);
    QNetworkReply *reply = manager->post(request, postData);
    eventLoop.exec();
    // means that the reply is ready to be read
    handleReply(reply);
    processStates(replyData);
    emit stateChanged(is_connected,user,used,total,left);
    //qDebug() << replyData;
}

void STULogin::handleReply(QNetworkReply *reply){
    //qDebug() << "handleReply " + QString::number(wrongCount);
    QByteArray response = reply->readAll();
    reply->deleteLater();
    QString info = codec->toUnicode(response);
    replyData = info;
    //qDebug() << used;
    manager->setNetworkAccessible(
                QNetworkAccessManager::Accessible);
}


void STULogin::getState(){
    //askingForState = true;
    QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(LOGIN_REQUEST_ADDR)));     // asynchronously
    QEventLoop eventLoop;   // for synchronization
    connect(manager, SIGNAL(finished(QNetworkReply*)),
            &eventLoop, SLOT(quit()));
    eventLoop.exec();
    // means that the reply is ready to be read
    handleReply(reply);
    processStates(replyData);
    emit stateChanged(is_connected,user,used,total,left);
}

void STULogin::processStates(const QString &data){
    // reset to default
    user = "";
    total = left = used = 0.0;
    is_connected = false;

    if (data.contains(CONNECTED)){
        is_connected = true;
        wrongCount = 0;
    }
    else if (/* data.contains(INVALID) && */ logining == true){
        logining = false;
        if (++wrongCount >= MAX_ERROR_COUNT){  // the server sucks
            if(!is_connected)
                QMessageBox::warning(0 ,trUtf8("错误"), trUtf8("账号或密码错误"), QMessageBox::Ok);
                qDebug() << "username/password may be wrong";
            wrongCount = 0;
            return;
        }

        login();
        return;
    }
    else{   // i.e. pressed logout button
        return;
    }
    QStringList info,states;
    info << USERNAME_POSITION << USEDBYTE_POSITION << TOTALBYTE_POSITION;
    int index, index_end;
    foreach (const QString &item, info) {
        index = data.indexOf(item) + item.length();
        index_end = data.indexOf(POSITION_END, index);
        states << data.mid(index, index_end - index);
    }
    user = states[0];

    used = states[1].replace(",","").toDouble() / CONVERT_RATE;     // from bytes to mb
    //qDebug() << used;
    total = states[2].replace(",","").toDouble() / CONVERT_RATE;

    left = total - used;

    if (autoChange == true && is_connected && left <= thresholdValue){
        if (stopChaneing == true)
            return;
        qDebug()<<"start changing account the original user is " + user + "\t and the flow_left is " + QString::number(left);
        changeAccount();
    }

}

void STULogin::clearErrorCount(){
    wrongCount = 0;
}

void STULogin::login(){
    //qDebug() << "timed out" + user + "\t" + passwd;
    login(user_copy,passwd);
}

void STULogin::setAccounts(const QList<Account> &accounts){
    this->allAccounts = accounts;
    stopChaneing = false;
    //qDebug() << AllAccounts.count();
}

void STULogin::setThresholdValue(int val){
    if (thresholdValue != val){  // new value
        thresholdValue = val;
        stopChaneing = false;
        resetAllAccounts();
    }
}

void STULogin::setAutoChangeState(bool state){
    autoChange = state;
    //qDebug() << "The state changed!!!" << state;
}

void STULogin::changeAccount(){
    int nextUserIndex = getNextFreshAccount();
    if (nextUserIndex == -1){
        qDebug() << "No more accounts available!";
        stopChaneing = true;
        return;
    }
    Account next = allAccounts.at(nextUserIndex);
    allAccounts[nextUserIndex].hasBeenUsed = true;
    logout();
    login(next.username, next.password);

}

void STULogin::delayForSomeTime(int ms){
    QEventLoop loop;
    delayTimer->setInterval(ms);
    delayTimer->setSingleShot(true);
    connect(delayTimer, SIGNAL(timeout()),
            &loop, SLOT(quit()));
    delayTimer->start();
    loop.exec();
}

int STULogin::getNextFreshAccount(){
   int count = allAccounts.count();
   for(int i = 0 ; i < count ; ++i){
       if (allAccounts[i].hasBeenUsed == false)
           return i;
   }
   return -1;
}

void STULogin::resetAllAccounts(){
    int count = allAccounts.count();
    for(int i = 0 ; i < count ; ++i)
        allAccounts[i].hasBeenUsed = false;
}
