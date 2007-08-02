/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 *   Copyright (C) 2007 by Dominik Riebeling
 *   $Id$
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include <QtGui>

#include "configure.h"
#include "ui_configurefrm.h"

#ifdef __linux
#include <stdio.h>
#endif

#define DEFAULT_LANG "English (builtin)"

Config::Config(QWidget *parent) : QDialog(parent)
{
    programPath = QFileInfo(qApp->arguments().at(0)).absolutePath() + "/";
    ui.setupUi(this);
    ui.radioManualProxy->setChecked(true);
    QRegExpValidator *proxyValidator = new QRegExpValidator(this);
    QRegExp validate("[0-9]*");
    proxyValidator->setRegExp(validate);
    ui.proxyPort->setValidator(proxyValidator);
#ifndef __linux
    ui.radioSystemProxy->setEnabled(false); // only on linux for now
#endif
    // build language list and sort alphabetically
    QStringList langs = findLanguageFiles();
    for(int i = 0; i < langs.size(); ++i)
        lang.insert(languageName(langs[i]), langs[i]);
    lang.insert(DEFAULT_LANG, "");
    QMap<QString, QString>::const_iterator i = lang.constBegin();
    while (i != lang.constEnd()) {
        ui.listLanguages->addItem(i.key());
        i++;
    }
    ui.listLanguages->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(ui.listLanguages, SIGNAL(itemSelectionChanged()), this, SLOT(updateLanguage()));
    ui.proxyPass->setEchoMode(QLineEdit::Password);

    this->setModal(true);
    
    connect(ui.buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(ui.buttonCancel, SIGNAL(clicked()), this, SLOT(abort()));
    connect(ui.radioNoProxy, SIGNAL(toggled(bool)), this, SLOT(setNoProxy(bool)));
    connect(ui.radioSystemProxy, SIGNAL(toggled(bool)), this, SLOT(setSystemProxy(bool)));
    connect(ui.browseMountPoint, SIGNAL(clicked()), this, SLOT(browseFolder()));

    // disable unimplemented stuff
    ui.buttonCacheBrowse->setEnabled(false);
    ui.cacheDisable->setEnabled(false);
    ui.cacheOfflineMode->setEnabled(false);
    ui.buttonCacheClear->setEnabled(false);

    ui.buttonAutodetect->setEnabled(false);
}


void Config::accept()
{
    qDebug() << "Config::accept()";
    // proxy: save entered proxy values, not displayed.
    if(ui.radioManualProxy->isChecked()) {
        proxy.setScheme("http");
        proxy.setUserName(ui.proxyUser->text());
        proxy.setPassword(ui.proxyPass->text());
        proxy.setHost(ui.proxyHost->text());
        proxy.setPort(ui.proxyPort->text().toInt());
    }
    userSettings->setValue("defaults/proxy", proxy.toString());
    qDebug() << "new proxy:" << proxy;
    // proxy type
    QString proxyType;
    if(ui.radioNoProxy->isChecked()) proxyType = "none";
    else if(ui.radioSystemProxy->isChecked()) proxyType = "system";
    else proxyType = "manual";
    userSettings->setValue("defaults/proxytype", proxyType);

    // language
    if(userSettings->value("defaults/lang").toString() != language)
        QMessageBox::information(this, tr("Language changed"),
            tr("You need to restart the application for the changed language to take effect."));
    userSettings->setValue("defaults/lang", language);

    // sync settings
    userSettings->sync();
    this->close();
    emit settingsUpdated();
}


void Config::abort()
{
    qDebug() << "Config::abort()";
    this->close();
}


void Config::setUserSettings(QSettings *user)
{
    userSettings = user;
    // set proxy
    QUrl proxy = userSettings->value("defaults/proxy").toString();

    ui.proxyPort->setText(QString("%1").arg(proxy.port()));
    ui.proxyHost->setText(proxy.host());
    ui.proxyUser->setText(proxy.userName());
    ui.proxyPass->setText(proxy.password());

    QString proxyType = userSettings->value("defaults/proxytype").toString();
    if(proxyType == "manual") ui.radioManualProxy->setChecked(true);
    else if(proxyType == "system") ui.radioSystemProxy->setChecked(true);
    else if(proxyType == "none") ui.radioNoProxy->setChecked(true);

    // set language selection
    QList<QListWidgetItem*> a;
    QString b;
    // find key for lang value
    QMap<QString, QString>::const_iterator i = lang.constBegin();
    while (i != lang.constEnd()) {
        if(i.value() == userSettings->value("defaults/lang").toString() + ".qm") {
            b = i.key();
            break;
        }
        i++;
    }
    a = ui.listLanguages->findItems(b, Qt::MatchExactly);
    if(a.size() <= 0)
        a = ui.listLanguages->findItems(DEFAULT_LANG, Qt::MatchExactly);
    if(a.size() > 0)
        ui.listLanguages->setCurrentItem(a.at(0));

    // devices tab
    ui.mountPoint->setText(userSettings->value("defaults/mountpoint").toString());

}


void Config::setDevices(QSettings *dev)
{
    devices = dev;
    // setup devices table
    qDebug() << "Config::setDevices()";
    devices->beginGroup("platforms");
    QStringList a = devices->childKeys();
    devices->endGroup();
    
    QMap <QString, QString> manuf;
    QMap <QString, QString> devcs;
    for(int it = 0; it < a.size(); it++) {
        QString curdev;
        devices->beginGroup("platforms");
        curdev = devices->value(a.at(it), "null").toString();
        devices->endGroup();
        QString curname;
        devices->beginGroup(curdev);
        curname = devices->value("name", "null").toString();
        QString curbrand = devices->value("brand", "").toString();
        devices->endGroup();
        manuf.insertMulti(curbrand, curdev);
        devcs.insert(curdev, curname);
    }

    QString platform;
    platform = devcs.value(userSettings->value("defaults/platform").toString());

    // set up devices table
    ui.treeDevices->header()->hide();
    ui.treeDevices->expandAll();
    ui.treeDevices->setColumnCount(1);
    QList<QTreeWidgetItem *> items;

    // get manufacturers
    QStringList brands = manuf.uniqueKeys();
    QTreeWidgetItem *w;
    QTreeWidgetItem *w2;
    QTreeWidgetItem *w3;
    for(int c = 0; c < brands.size(); c++) {
        qDebug() << brands.at(c);
        w = new QTreeWidgetItem();
        w->setFlags(Qt::ItemIsEnabled);
        w->setText(0, brands.at(c));
//        w->setData(0, Qt::DecorationRole, <icon>);
        items.append(w);
        
        // go through platforms again for sake of order
        for(int it = 0; it < a.size(); it++) {
            QString curdev;
            devices->beginGroup("platforms");
            curdev = devices->value(a.at(it), "null").toString();
            devices->endGroup();
            QString curname;
            devices->beginGroup(curdev);
            curname = devices->value("name", "null").toString();
            QString curbrand = devices->value("brand", "").toString();
            devices->endGroup();
            if(curbrand != brands.at(c)) continue;
            qDebug() << "adding:" << brands.at(c) << curname << curdev;
            w2 = new QTreeWidgetItem(w, QStringList(curname));
            w2->setData(0, Qt::UserRole, curdev);
            if(platform.contains(curname)) {
                w2->setSelected(true);
                w->setExpanded(true);
                w3 = w2; // save pointer to hilight old selection
            }
            items.append(w2);
        }
    }
    ui.treeDevices->insertTopLevelItems(0, items);
    ui.treeDevices->setCurrentItem(w3); // hilight old selection
    connect(ui.treeDevices, SIGNAL(itemSelectionChanged()), this, SLOT(updatePlatform()));
}


void Config::updatePlatform()
{
    qDebug() << "updatePlatform()";
    QString nplat;
    nplat = ui.treeDevices->selectedItems().at(0)->data(0, Qt::UserRole).toString();
    userSettings->setValue("defaults/platform", nplat);
}


void Config::setNoProxy(bool checked)
{
    bool i = !checked;
    ui.proxyPort->setEnabled(i);
    ui.proxyHost->setEnabled(i);
    ui.proxyUser->setEnabled(i);
    ui.proxyPass->setEnabled(i);
}


void Config::setSystemProxy(bool checked)
{
    bool i = !checked;
    ui.proxyPort->setEnabled(i);
    ui.proxyHost->setEnabled(i);
    ui.proxyUser->setEnabled(i);
    ui.proxyPass->setEnabled(i);
    if(checked) {
        // save values in input box
        proxy.setScheme("http");
        proxy.setUserName(ui.proxyUser->text());
        proxy.setPassword(ui.proxyPass->text());
        proxy.setHost(ui.proxyHost->text());
        proxy.setPort(ui.proxyPort->text().toInt());
        // show system values in input box
#ifdef __linux
        QUrl envproxy = QUrl(getenv("http_proxy"));
        ui.proxyHost->setText(envproxy.host());
        ui.proxyPort->setText(QString("%1").arg(envproxy.port()));
        ui.proxyUser->setText(envproxy.userName());
        ui.proxyPass->setText(envproxy.password());
#endif
    }
    else {
        ui.proxyHost->setText(proxy.host());
        ui.proxyPort->setText(QString("%1").arg(proxy.port()));
        ui.proxyUser->setText(proxy.userName());
        ui.proxyPass->setText(proxy.password());
    }

}


QStringList Config::findLanguageFiles()
{
    QDir dir(programPath + "/");
    QStringList fileNames;
    fileNames = dir.entryList(QStringList("*.qm"), QDir::Files, QDir::Name);

    QDir resDir(":/lang");
    fileNames += resDir.entryList(QStringList("*.qm"), QDir::Files, QDir::Name);

    fileNames.sort();
    qDebug() << "Config::findLanguageFiles()" << fileNames;

    return fileNames;
}


QString Config::languageName(const QString &qmFile)
{
    QTranslator translator;

    if(!translator.load(qmFile, programPath))
        translator.load(qmFile, ":/lang");

    return translator.translate("Configure", "English");
}


void Config::updateLanguage()
{
    qDebug() << "updateLanguage()";
    QList<QListWidgetItem*> a = ui.listLanguages->selectedItems();
    if(a.size() > 0)
        language = QFileInfo(lang.value(a.at(0)->text())).baseName();
}


void Config::browseFolder()
{
    QFileDialog browser(this);
    if(QFileInfo(ui.mountPoint->text()).isDir())
        browser.setDirectory(ui.mountPoint->text());
    else
        browser.setDirectory("/media");
    browser.setReadOnly(true);
    browser.setFileMode(QFileDialog::DirectoryOnly);
    browser.setAcceptMode(QFileDialog::AcceptOpen);
    if(browser.exec()) {
        qDebug() << browser.directory();
        QStringList files = browser.selectedFiles();
        ui.mountPoint->setText(files.at(0));
        userSettings->setValue("defaults/mountpoint", files.at(0));
    }
}
