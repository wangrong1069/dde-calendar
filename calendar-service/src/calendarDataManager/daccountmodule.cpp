/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     chenhaifeng  <chenhaifeng@uniontech.com>
*
* Maintainer: chenhaifeng  <chenhaifeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "daccountmodule.h"
#include "units.h"
#include "dscheduletype.h"
#include "dschedulequerypar.h"
#include "memorycalendar.h"
#include "lunardateinfo.h"
#include "lunarmanager.h"
#include "dalarmmanager.h"

#include <QStringList>

#define UPDATEREMINDJOBTIMEINTERVAL 1000 * 60 * 10 //提醒任务更新时间间隔毫秒数（10分钟）
DAccountModule::DAccountModule(const DAccount::Ptr &account, QObject *parent)
    : QObject(parent)
    , m_account(account)
    , m_accountDB(new DAccountDataBase(account))
{
    QString newDbPatch = getDBPath();
    m_accountDB->setDBPath(newDbPatch + "/" + account->dbName());
    m_accountDB->dbOpen();
    m_accountDB->initDBData();
    m_account = m_accountDB->getAccountInfo();
}

QString DAccountModule::getAccountInfo()
{
    QString accountInfo;
    m_account = m_accountDB->getAccountInfo();
    DAccount::toJsonString(m_account, accountInfo);
    return accountInfo;
}

QString DAccountModule::getScheduleTypeList()
{
    DScheduleType::List typeList = m_accountDB->getScheduleTypeList();
    QString typeListStr;
    DScheduleType::toJsonListString(typeList, typeListStr);
    return typeListStr;
}

QString DAccountModule::getScheduleTypeByID(const QString &typeID)
{
    DScheduleType::Ptr scheduleType = m_accountDB->getScheduleTypeByID(typeID);
    QString typeStr;
    DScheduleType::toJsonString(scheduleType, typeStr);
    return typeStr;
}

QString DAccountModule::createScheduleType(const QString &typeInfo)
{
    DScheduleType::Ptr scheduleType;
    DScheduleType::fromJsonString(scheduleType, typeInfo);
    //如果颜色为用户自定义则需要在数据库中记录
    if (scheduleType->typeColor().privilege() == DTypeColor::PriUser) {
        m_accountDB->addTypeColor(scheduleType->typeColor().colorID(), scheduleType->typeColor().colorCode(), 7);
    }
    return m_accountDB->createScheduleType(scheduleType);
}

bool DAccountModule::deleteScheduleTypeByID(const QString &typeID)
{
    //根据帐户是否为网络帐户需要添加任务列表中,并设置弱删除
    if (m_account->isNetWorkAccount()) {
        QStringList scheduleIDList = m_accountDB->getScheduleIDListByTypeID(typeID);
        //弱删除
        m_accountDB->deleteSchedulesByScheduleTypeID(typeID);
        m_accountDB->deleteScheduleTypeByID(typeID);
        //发送操作内容给任务列表

    } else {
        m_accountDB->deleteSchedulesByScheduleTypeID(typeID, 1);
        m_accountDB->deleteScheduleTypeByID(typeID, 1);
    }
    //TODO
    //删除提醒任务和关闭对应弹窗

    //如果为用户颜色则删除颜色
    //删除日程类型
    return true;
}

bool DAccountModule::scheduleTypeByUsed(const QString &typeID)
{
    return m_accountDB->scheduleTypeByUsed(typeID);
}

QString DAccountModule::createSchedule(const QString &scheduleInfo)
{
    DSchedule::Ptr schedule;
    DSchedule::fromJsonString(schedule, scheduleInfo);
    schedule->setCreated(QDateTime::currentDateTime());
    //根据是否为提醒日程更新提醒任务
    if (schedule->alarms().size() > 0) {
        //TODO:
    }
    QString scheduleID = m_accountDB->createSchedule(schedule);
    //根据是否为网络帐户判断是否需要更新任务列表
    if (m_account->isNetWorkAccount()) {
        //TODO:
    }
    //发送日程更新信号
    emit signalScheduleUpdate();
    return scheduleID;
}

bool DAccountModule::updateSchedule(const QString &scheduleInfo)
{
    //TODO:根据是否为网络帐户判断是否需要
    //TODO:根据是否为提醒日程更新提醒任务,修改前的和修改后的
    DSchedule::Ptr schedule;
    DSchedule::fromJsonString(schedule, scheduleInfo);
    DSchedule::Ptr oldSchedule = m_accountDB->getScheduleByScheduleID(schedule->uid());
    schedule->setLastModified(QDateTime::currentDateTime());
    schedule->setRevision(schedule->revision() + 1);

    //根据日程ID获取提醒日程信息

    //如果是重复日程且重复规则不一样
    if (schedule->recurs() && *schedule->recurrence() != *oldSchedule->recurrence()) {
    }
    //

    bool ok = m_accountDB->updateSchedule(schedule);

    return ok;
}

QString DAccountModule::getScheduleByScheduleID(const QString &scheduleID)
{
    DSchedule::Ptr schedule = m_accountDB->getScheduleByScheduleID(scheduleID);
    QString scheduleStr;
    DSchedule::toJsonString(schedule, scheduleStr);
    return scheduleStr;
}

bool DAccountModule::deleteScheduleByScheduleID(const QString &scheduleID)
{
    //TODO:根据是否为网络判断是否需要弱删除
    //TODO:根据是否为提醒日程更新提醒任务,
    if (m_account->isNetWorkAccount()) {
    } else {
    }
    //关闭提醒消息和对应的通知弹框
}

QString DAccountModule::querySchedulesWithParameter(const QString &params)
{
    DScheduleQueryPar::Ptr queryPar = DScheduleQueryPar::fromJsonString(params);
    if (queryPar.isNull()) {
        return QString();
    }
    DSchedule::List scheduleList;
    if (queryPar->queryType() == DScheduleQueryPar::Query_RRule) {
        scheduleList = m_accountDB->querySchedulesByRRule(queryPar->key(), queryPar->queryType());
    } else {
        scheduleList = m_accountDB->querySchedulesByKey(queryPar->key());
    }

    bool extend = queryPar->queryType() == DScheduleQueryPar::Query_None;
    //根据条件判断是否需要添加节假日日程
    if (isChineseEnv() && extend) {
        scheduleList.append(getFestivalSchedule(queryPar->dtStart(), queryPar->dtEnd(), queryPar->key()));
    }

    //获取一定范围内的日程
    QMap<QDate, DSchedule::List> scheduleMap = getScheduleTimesOn(queryPar->dtStart(), queryPar->dtEnd(), scheduleList, extend);

    //如果为查询前N个日程，则取前N个日程
    if (queryPar->queryType() == DScheduleQueryPar::Query_Top) {
        //TODO:
    }
    return DSchedule::toMapString(scheduleMap);
}

DSchedule::List DAccountModule::getRemindScheduleList(const QDateTime &dtStart, const QDateTime &dtEnd)
{
    //获取范围内需要提醒的日程信息
    DSchedule::List scheduleList;
    //当前最多提前一周提醒。所以结束时间+8天
    QMap<QDate, DSchedule::List> scheduleMap = getScheduleTimesOn(dtStart, dtEnd.addDays(8), m_accountDB->getRemindSchedule(), false);
    QMap<QDate, DSchedule::List>::const_iterator iter = scheduleMap.constBegin();
    for (; iter != scheduleMap.constEnd(); ++iter) {
        foreach (auto schedule, iter.value()) {
            if (schedule->alarms().size() > 0
                && schedule->alarms()[0]->time() >= dtStart && schedule->alarms()[0]->time() <= dtEnd) {
                scheduleList.append(schedule);
            }
        }
    }
    return scheduleList;
}

QString DAccountModule::getSysColors()
{
    DTypeColor::List colorList = m_accountDB->getSysColor();
    return DTypeColor::toJsonString(colorList);
}

DAccount::Ptr DAccountModule::account() const
{
    return m_account;
}

void DAccountModule::updateRemindSchedules(bool isClear)
{
    QDateTime dtStart = QDateTime::currentDateTime();
    QDateTime dtEnd = dtStart.addMSecs(UPDATEREMINDJOBTIMEINTERVAL);

    //获取未提醒的日程相关信息
    DRemindData::List noRemindList = m_accountDB->getValidRemindJob();

    //获取每个账户下需要提醒的日程信息
    DRemindData::List accountRemind;

    DSchedule::List scheduleList = getRemindScheduleList(dtStart, dtEnd);
    foreach (auto &remind, scheduleList) {
        DRemindData::Ptr remindData = DRemindData::Ptr(new DRemindData);
        remindData->setScheduleID(remind->uid());
        remindData->setAccountID(m_account->accountID());
        remindData->setDtStart(remind->dtStart());
        remindData->setDtEnd(remind->dtEnd());
        remindData->setRecurrenceId(remind->recurrenceId());
        //由于获取的都是需要提醒的日程,所以日程的提醒列表size必然是大于0的
        remindData->setDtRemind(remind->alarms()[0]->time());
        remindData->setNotifyid(0);
        remindData->setRemindCount(0);
        accountRemind.append(remindData);
    }

    //获取未提醒的稍后日程信息,由于15分钟后，1xxx后等不会修改提醒次数
    //所以需要根据提醒时间，日程id，日程重复id来判断是否是没有被触发点提醒日程
    for (int i = 0; i < noRemindList.size(); i++) {
        for (int j = accountRemind.size() - 1; j < 0; j--) {
            if (accountRemind.at(j)->scheduleID() == noRemindList.at(i)->scheduleID()
                && accountRemind.at(j)->recurrenceId() == noRemindList.at(i)->recurrenceId()
                && accountRemind.at(j)->dtRemind() == noRemindList.at(i)->dtRemind())
                //如果该日程没有被触发提醒过(创建后没有被提醒，而不是提醒后点了15分钟后等不改变提醒次数的日程)
                //则移除
                accountRemind.removeAt(j);
        }
    }

    if (isClear) {
        //清空数据库
        m_accountDB->clearRemindJobDatabase();

        foreach (auto remind, noRemindList) {
            m_accountDB->createRemindInfo(remind);
        }
    }
    //添加从账户中获取到的需要提醒的日程信息
    foreach (auto remind, accountRemind) {
        m_accountDB->createRemindInfo(remind);
    }
    accountRemind.append(noRemindList);

    DAlarmManager alarmManager;
    //更新提醒任务
    alarmManager.updateRemind(accountRemind);
}

void DAccountModule::notifyMsgHanding(const QString &alarmID, const qint32 operationNum)
{
    DRemindData::Ptr remindData = m_accountDB->getRemindData(alarmID);
    remindData->setAccountID(m_account->accountID());
    //如果相应的日程被删除,则不做处理
    if (remindData.isNull()) {
        return;
    }
    //如果为稍后提醒操作则需要更新对应的重复次数和提醒时间
    qint64 Minute = 60 * 1000;
    qint64 Hour = Minute * 60;
    switch (operationNum) {
    case 2: { //稍后提醒
        remindData->setRemindCount(remindData->remindCount() + 1);
        remindData->updateRemindTimeByCount();
        m_accountDB->updateRemindInfo(remindData);
    } break;
    case 21: { //15min后提醒
        remindData->updateRemindTimeByMesc(15 * Minute);
        m_accountDB->updateRemindInfo(remindData);
    } break;
    case 22: { //一个小时后提醒
        remindData->updateRemindTimeByMesc(Hour);
        m_accountDB->updateRemindInfo(remindData);
    } break;
    case 23: { //四个小时后提醒
        remindData->updateRemindTimeByMesc(4 * Hour);
        m_accountDB->updateRemindInfo(remindData);
    } break;
    case 3: { //明天提醒
        remindData->updateRemindTimeByMesc(24 * Hour);
        m_accountDB->updateRemindInfo(remindData);
    } break;
    case 1: //打开日历
    case 4: { //提前1天提醒
        DSchedule::Ptr schedule = m_accountDB->getScheduleByScheduleID(remindData->scheduleID());

        if (schedule->allDay()) {
            schedule->setAlarmType(DSchedule::Alarm_15Min_Front);
        } else {
            schedule->setAlarmType(DSchedule::Alarm_1Day_Front);
        }
        m_accountDB->updateSchedule(schedule);
    } break;
    default:
        //删除对应的数据
        m_accountDB->deleteRemindInfoByAlarmID(alarmID);
        break;
    }
    DAlarmManager alarm;
    alarm.notifyMsgHanding(remindData, operationNum);
}

void DAccountModule::remindJob(const QString &alarmID)
{
    DRemindData::Ptr remindData = m_accountDB->getRemindData(alarmID);
    remindData->setAccountID(m_account->accountID());
    DSchedule::Ptr schedule = m_accountDB->getScheduleByScheduleID(remindData->scheduleID());
    schedule->setDtStart(remindData->dtStart());
    schedule->setDtEnd(remindData->dtEnd());
    schedule->setRecurrenceId(remindData->recurrenceId());
    DAlarmManager alarm;
    int notifyid = alarm.remindJob(remindData, schedule);
    remindData->setNotifyid(notifyid);
    m_accountDB->updateRemindInfo(remindData);
}

QMap<QDate, DSchedule::List> DAccountModule::getScheduleTimesOn(const QDateTime &dtStart, const QDateTime &dtEnd, const DSchedule::List &scheduleList, bool extend)
{
    QMap<QDate, DSchedule::List> m_scheduleMap;
    //相差多少天
    int days = static_cast<int>(dtStart.daysTo(dtEnd));
    for (int i = 0; i <= days; ++i) {
        DSchedule::List scheduleList;
        m_scheduleMap[dtStart.addDays(i).date()] = scheduleList;
    }

    foreach (auto &schedule, scheduleList) {
        //获取日程的开始结束时间差
        qint64 interval = schedule->dtStart().secsTo(schedule->dtEnd());
        //如果存在重复日程
        if (schedule->recurs()) {
            //如果为农历日程
            if (schedule->lunnar()) {
                //农历重复日程计算
                LunarDateInfo lunardate(schedule->recurrence()->defaultRRuleConst(), interval);

                QMap<int, QDate> ruleStartDate = lunardate.getRRuleStartDate(dtStart.date(), dtEnd.date(), schedule->dtStart().date());

                QDateTime recurDateTime;
                recurDateTime.setTime(schedule->dtStart().time());
                QDateTime copyEnd;
                QMap<int, QDate>::ConstIterator iter = ruleStartDate.constBegin();
                for (; iter != ruleStartDate.constEnd(); iter++) {
                    recurDateTime.setDate(iter.value());
                    //如果在忽略时间列表中,则忽略
                    if (schedule->recurrence()->exDateTimes().contains(recurDateTime))
                        continue;
                    copyEnd = recurDateTime.addSecs(interval);
                    DSchedule::Ptr newSchedule = DSchedule::Ptr(new DSchedule(*schedule.data()));
                    newSchedule->setDtStart(recurDateTime);
                    newSchedule->setDtEnd(copyEnd);
                    //只有重复日程设置RecurrenceId
                    if (schedule->dtStart() != recurDateTime) {
                        newSchedule->setRecurrenceId(recurDateTime);
                    }

                    if (extend) {
                        //需要扩展的天数
                        int extenddays = static_cast<int>(recurDateTime.daysTo(copyEnd));
                        for (int i = 0; i <= extenddays; ++i) {
                            m_scheduleMap[recurDateTime.date().addDays(i)].append(newSchedule);
                        }

                    } else {
                        m_scheduleMap[recurDateTime.date()].append(newSchedule);
                    }
                }
            } else {
                //非农历日程
                QList<QDateTime> dtList = schedule->recurrence()->timesInInterval(dtStart, dtEnd);
                foreach (auto &dt, dtList) {
                    QDateTime scheduleDtEnd = dt.addSecs(interval);
                    DSchedule::Ptr newSchedule = DSchedule::Ptr(schedule->clone());
                    newSchedule->setDtStart(dt);
                    newSchedule->setDtEnd(scheduleDtEnd);

                    //只有重复日程设置RecurrenceId
                    if (schedule->dtStart() != dt) {
                        newSchedule->setRecurrenceId(dt);
                    }
                    if (extend) {
                        //需要扩展的天数
                        int extenddays = static_cast<int>(dt.daysTo(scheduleDtEnd));
                        for (int i = 0; i <= extenddays; ++i) {
                            m_scheduleMap[dt.date().addDays(i)].append(newSchedule);
                        }

                    } else {
                        m_scheduleMap[dt.date()].append(newSchedule);
                    }
                }
            }
        } else {
            //普通日程
            //如果在查询时间范围内
            if (!(schedule->dtEnd() < dtStart || schedule->dtStart() > dtEnd)) {
                if (extend && schedule->isMultiDay()) {
                    //需要扩展的天数
                    int extenddays = static_cast<int>(schedule->dtStart().daysTo(schedule->dtEnd()));
                    for (int i = 0; i <= extenddays; ++i) {
                        m_scheduleMap[schedule->dtStart().date().addDays(i)].append(schedule);
                    }
                } else {
                    m_scheduleMap[schedule->dtStart().date()].append(schedule);
                }
            }
        }
    }
    return m_scheduleMap;
}

DSchedule::List DAccountModule::getFestivalSchedule(const QDateTime &dtStart, const QDateTime &dtEnd, const QString &key)
{
    QList<stDayFestival> festivaldays = GetFestivalsInRange(dtStart, dtEnd);
    if (!key.isEmpty()) {
        festivaldays = FilterDayFestival(festivaldays, key);
    }
    DSchedule::List scheduleList;
    foreach (stDayFestival festivalDay, festivaldays) {
        foreach (QString festival, festivalDay.Festivals) {
            if (!festival.isEmpty()) {
                DSchedule::Ptr schedule = DSchedule::Ptr(new DSchedule);
                schedule->setSummary(festival);
                schedule->setAllDay(true);
                schedule->setDtStart(QDateTime(QDate(festivalDay.date.date()), QTime(0, 0)));
                schedule->setDtEnd(QDateTime(QDate(festivalDay.date.date()), QTime(23, 59)));
                schedule->setUid(DDataBase::createUuid());
                schedule->setScheduleTypeID(m_accountDB->getFestivalTypeID());
                scheduleList.append(schedule);
            }
        }
    }
    return scheduleList;
}