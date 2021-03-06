/*
 *   Copyright 2018 Camilo Higuita <milo.h@aol.com>
 *   Copyright 2021 Zhang He Gang <zhanghegang@jingos.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "tagdb.h"
#include "fmh.h"
#include <QUuid>

TAGDB::TAGDB()
    : QObject(nullptr)
{
    QDir collectionDBPath_dir(TAG::TaggingPath);
    if (!collectionDBPath_dir.exists()) {
        collectionDBPath_dir.mkpath(".");
    }

    this->name = QUuid::createUuid().toString();
    if (!FMH::fileExists(QUrl::fromLocalFile(TAG::TaggingPath + TAG::DBName))) {
        this->openDB(this->name);
        this->prepareCollectionDB();
    } else {
        this->openDB(this->name);
    }
}

TAGDB::~TAGDB()
{
    qDebug() << "CLOSING THE TAGGING DATA BASE";
    this->m_db.close();
}

void TAGDB::openDB(const QString &name)
{
    if (!QSqlDatabase::contains(name)) {
        this->m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        this->m_db.setDatabaseName(TAG::TaggingPath + TAG::DBName);
    }

    if (!this->m_db.isOpen()) {
        if (!this->m_db.open()) {
            qWarning() << "ERROR OPENING DB" << this->m_db.lastError().text() << m_db.connectionName();
        }
    }
    auto query = this->getQuery("PRAGMA synchronous=OFF");
    query.exec();
}

void TAGDB::prepareCollectionDB() const
{
    QSqlQuery query(this->m_db);

    QFile file(":/script.sql");

    if (!file.exists()) {
        QString log = QStringLiteral("Fatal error on build database. The file '");
        log.append(file.fileName() + QStringLiteral("' for database and tables creation query cannot be not found!"));
        qDebug() << log;
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << QStringLiteral("Fatal error on try to create database! The file with sql queries for database creation cannot be opened!");
        return;
    }

    bool hasText;
    QString line;
    QByteArray readLine;
    QString cleanedLine;
    QStringList strings;

    while (!file.atEnd()) {
        hasText = false;
        line = "";
        readLine = "";
        cleanedLine = "";
        strings.clear();
        while (!hasText) {
            readLine = file.readLine();
            cleanedLine = readLine.trimmed();
            strings = cleanedLine.split("--");
            cleanedLine = strings.at(0);
            if (!cleanedLine.startsWith("--") && !cleanedLine.startsWith("DROP") && !cleanedLine.isEmpty()) {
                line += cleanedLine;
            }
            if (cleanedLine.endsWith(";")) {
                break;
            }
            if (cleanedLine.startsWith("COMMIT")) {
                hasText = true;
            }
        }
        if (!line.isEmpty()) {
            if (!query.exec(line)) {
                qDebug() << "exec failed" << query.lastQuery() << query.lastError();
            }

        } else {
            qDebug() << "exec wrong" << query.lastError();
        }
    }
    file.close();
}

bool TAGDB::checkExistance(const QString &tableName, const QString &searchId, const QString &search)
{
    const auto queryStr = QString("SELECT %1 FROM %2 WHERE %3 = \"%4\"").arg(searchId, tableName, searchId, search);
    return this->checkExistance(queryStr);
}

bool TAGDB::checkExistance(const QString &queryStr)
{
    auto query = this->getQuery(queryStr);

    if (query.exec()) {
        if (query.next()) {
            return true;
        }
    } else {
        qDebug() << query.lastError().text();
    }

    return false;
}

QSqlQuery TAGDB::getQuery(const QString &queryTxt)
{
    QSqlQuery query(queryTxt, this->m_db);
    return query;
}

bool TAGDB::insert(const QString &tableName, const QVariantMap &insertData)
{
    if (tableName.isEmpty()) {
        return false;

    } else if (insertData.isEmpty()) {
        return false;
    }

    QStringList strValues;
    QStringList fields = insertData.keys();
    QVariantList values = insertData.values();
    int totalFields = fields.size();
    for (int i = 0; i < totalFields; ++i) {
        strValues.append("?");
    }

    QString sqlQueryString = "INSERT INTO " + tableName + " (" + QString(fields.join(",")) + ") VALUES(" + QString(strValues.join(",")) + ")";
    QSqlQuery query(this->m_db);
    query.prepare(sqlQueryString);

    int k = 0;
    foreach (const QVariant &value, values) {
        query.bindValue(k++, value);
    }

    return query.exec();
}

bool TAGDB::insertDatas(const QString &tableName, const QList<QHash<QString, QString>> &insertDatas)
{
    if (tableName.isEmpty()) {
        return false;

    } else if (insertDatas.isEmpty()) {
        return false;
    }

    QString sqlQueryString = "INSERT INTO " + tableName;
    QStringList fields;
    for (int i = 0; i < insertDatas.size() ; i++) {
        QHash<QString, QString> insertData = insertDatas.at(i);
        if (i == 0) {
            fields = insertData.keys();
            sqlQueryString.append(" (" + QString(fields.join(",")) + ") VALUES");
        }
        QList<QString> values = insertData.values();
        QString sqlValues;
        for (int j = 0; j < values.size() ; j++) {
            sqlValues.append("\""+values.at(j)+"\"");
            if (j < values.size() - 1) {
                sqlValues.append(",");
            }
        }
        sqlQueryString.append(" (" +sqlValues + ") ");
        if (i < insertDatas.size() - 1 && insertDatas.size() > 1) {
            sqlQueryString.append(",");
        }

    }
    QSqlQuery query(this->m_db);
    query.prepare(sqlQueryString);
    return query.exec();
}

bool TAGDB::update(const QString &tableName, const FMH::MODEL &updateData, const QVariantMap &where)
{
    if (tableName.isEmpty()) {
        return false;
    } else if (updateData.isEmpty()) {
        return false;
    }

    QStringList set;
    for (auto key : updateData.keys()) {
        set.append(FMH::MODEL_NAME[key] + " = '" + updateData[key] + "'");
    }

    QStringList condition;
    for (auto key : where.keys()) {
        condition.append(key + " = '" + where[key].toString() + "'");
    }

    QString sqlQueryString = "UPDATE " + tableName + " SET " + QString(set.join(",")) + " WHERE " + QString(condition.join(","));
    auto query = this->getQuery(sqlQueryString);
    return query.exec();
}

bool TAGDB::update(const QString &table, const QString &column, const QVariant &newValue, const QVariant &op, const QString &id)
{
    auto queryStr = QString("UPDATE %1 SET %2 = \"%3\" WHERE %4 = \"%5\"").arg(table, column, newValue.toString().replace("\"", "\"\""), op.toString(), id);
    auto query = this->getQuery(queryStr);
    return query.exec();
}

bool TAGDB::remove(const QString &tableName, const FMH::MODEL &removeData)
{
    if (tableName.isEmpty()) {
        return false;

    } else if (removeData.isEmpty()) {
        return false;
    }

    QString strValues;
    auto i = 0;
    for (auto key : removeData.keys()) {
        strValues.append(QString("%1 = \"%2\"").arg(FMH::MODEL_NAME[key], removeData[key]));
        i++;

        if (removeData.size() > 1 && i < removeData.size()) {
            strValues.append(" AND ");
        }
    }

    QString sqlQueryString = "DELETE FROM " + tableName + " WHERE " + strValues;
    return this->getQuery(sqlQueryString).exec();
}

bool TAGDB::remove(const QString &tableName, const QList<FMH::MODEL> &removeDatas)
{
    if (tableName.isEmpty()) {
        return false;

    } else if (removeDatas.isEmpty()) {
        return false;
    }

    QString strValues;
    for (int j = 0 ; j < removeDatas.size() ; j++) {
        FMH::MODEL removeData = removeDatas.at(j);
        auto i = 0;
        strValues.append("(");
        for (auto key : removeData.keys()) {
            if (key == FMH::MODEL_KEY::TAG) {
                strValues.append(QString("%1 LIKE \"tag%\"").arg(FMH::MODEL_NAME[key]));
            } else {
                strValues.append(QString("%1 = \"%2\"").arg(FMH::MODEL_NAME[key], removeData[key]));
            }
            i++;

            if (removeData.size() > 1 && i < removeData.size()) {
                strValues.append(" AND ");
            }
        }
        strValues.append(")");
        if (j < removeDatas.size() - 1) {
            strValues.append(" OR ");
        }
    }


    QString sqlQueryString = "DELETE FROM " + tableName + " WHERE " + strValues;

    return this->getQuery(sqlQueryString).exec();
}
