/* Double Contact
 *
 * Module: Main Window
 *
 * Copyright 2016 Mikhail Y. Zvyozdochkin aka DarkHobbit <pub@zvyozdochkin.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING file for more details.
 *
 */

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QTranslator>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "contactdialog.h"
#include "comparedialog.h"
#include "multicontactdialog.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    modLeft = new ContactModel(this, S_NEW_LIST);
    modRight = new ContactModel(this, S_NEW_LIST + " 2");
    proxyLeft  = new ContactSorterFilter(this);
    proxyRight  = new ContactSorterFilter(this);
    /*ui->tvLeft->setModel(modLeft);
    ui->tvRight->setModel(modRight);*/
    proxyLeft->setSourceModel(modLeft);
    proxyRight->setSourceModel(modRight);
    proxyLeft->setFilterKeyColumn(-1);
    proxyRight->setFilterKeyColumn(-1);
    ui->tvLeft->setModel(proxyLeft);
    ui->tvRight->setModel(proxyRight);
    // Status bar
    lbMode = new QLabel(0);
    statusBar()->addWidget(lbMode);
    // Settings
    setDlg = new SettingsDialog(0);
    setDlg->readConfig();
    setLanguage(setDlg->lang());
    // Track selected view
    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(anyFocusChanged(QWidget*,QWidget*)));
    setSelectionModelEvents();
    connect(ui->tvLeft, SIGNAL(doubleClicked (const QModelIndex&)), this, SLOT(rowDoubleClicked(const QModelIndex&)));
    connect(ui->tvRight, SIGNAL(doubleClicked (const QModelIndex&)), this, SLOT(rowDoubleClicked(const QModelIndex&)));
    selectView(ui->tvLeft);
    selection = selectedView->selectionModel()->selectedRows();
    // Test data
    if (qApp->arguments().contains("-d"))
        modLeft->testList();
    // File command-line data
    else if (qApp->arguments().count()>1 && !qApp->arguments().contains("-q")) {
        modLeft->open(qApp->arguments()[1]);
        if (qApp->arguments().count()>2) {
            ui->action_Two_panels->setChecked(true);
            modRight->open(qApp->arguments()[2]);
        }
    }
    // Previous session file data
    else if (!qApp->arguments().contains("-q")
             && setDlg->openLastFilesAtStartup()
             && QFile(setDlg->lastPath()).exists()
             && !(QFileInfo(setDlg->lastPath()).isDir()))
        selectedModel->open(setDlg->lastPath());
    // Show data
    ui->action_Two_panels->setChecked(setDlg->showTwoPanels());
    ui->action_Sort->setChecked(setDlg->sortingEnabled());
    setSorting(setDlg->sortingEnabled());
    updateHeaders();
    updateModeStatus();
    on_action_Two_panels_toggled(ui->action_Two_panels->isChecked());
}

MainWindow::~MainWindow()
{
    delete setDlg;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    askSaveChanges(event, modLeft);
    askSaveChanges(event, modRight);
}

void MainWindow::showEvent(QShowEvent*)
{
    updateConfig();
}

void MainWindow::anyFocusChanged(QWidget *, QWidget *now)
{
    if ((now==ui->tvLeft)||(now==ui->tvRight))
        selectView(dynamic_cast<QTableView*>(now));
    setButtonsAccess();
    updateHeaders();
}

void MainWindow::on_actionE_xit_triggered()
{
    close();
}

void MainWindow::on_action_Two_panels_toggled(bool showTwoPanels)
{
    if (showTwoPanels) {
        ui->tvRight->show();
        ui->lbRight->show();
    }
    else {
        ui->tvRight->hide();
        ui->lbRight->hide();
        selectView(ui->tvLeft);
    }
    setButtonsAccess();
    setDlg->setShowTwoPanels(showTwoPanels);
    updateModeStatus();
}

void MainWindow::on_btnExit_clicked()
{
    close();
}

// Open
void MainWindow::on_action_Open_triggered()
{
    // TODO ask if model has unsaved changes (after save implementation)
    QString selectedFilter;
    QString path = QFileDialog::getOpenFileName(0, tr("Open contact file"),
        setDlg->lastPath(), FormatFactory::supportedFilters().join(";;"), &selectedFilter);
    if (!path.isEmpty()) {
        selectedModel->open(path);
        setDlg->setLastPath(path);
        updateHeaders();
    }
}

// Save
void MainWindow::on_action_Save_triggered()
{
    // TODO check if list was loaded from file, else call save as
    selectedModel->saveAs(selectedModel->source());
}

// Save as
void MainWindow::on_actionSave_as_triggered()
{
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(0, tr("Save contact file"),
        setDlg->lastPath(), FormatFactory::supportedFilters().join(";;"), &selectedFilter);
    if (!path.isEmpty()) {
        selectedModel->saveAs(path);
        setDlg->setLastPath(path);
        updateHeaders();
    }
}

// Add
void MainWindow::on_action_Add_triggered()
{
    ContactDialog* d = new ContactDialog(0);
    d->clearData();
    d->exec();
    if (d->result()==QDialog::Accepted) {
        ContactItem c;
        d->getData(c);
        selectedModel->addRow(c);
        updateViewMode();
    }
    delete d;
}

void MainWindow::on_btnAdd_clicked()
{
    on_action_Add_triggered();
}

// Edit
void MainWindow::on_action_Edit_triggered()
{
    if (!checkSelection(true/*, true TODO change errmsg or drop 2nd checkSelection param and check in comparizon */)) return;
    if (selection.count()==1) // Ordinary edition
    {
        ContactDialog* d = new ContactDialog(0);
        ContactItem& c = selectedModel->beginEditRow(selection[0]);
        d->setData(c);
        d->exec();
        if (d->result()==QDialog::Accepted) {
            d->getData(c);
            selectedModel->endEditRow(selection[0]);
            updateViewMode();
        }
        delete d;
    }
    else
    if (QMessageBox::question(0, S_CONFIRM, tr("Are You really want to edit more than one record?\nOnly some fields can this edited in this mode"),
                QMessageBox::Yes, QMessageBox::No)==QMessageBox::Yes)
    { // Multi-record edition
        MultiContactDialog* d = new MultiContactDialog(0);
        for (int i=0; i<selection.count(); i++)
            d->setData(selectedModel->beginEditRow(selection[i]), i==0);
        d->exec();
        if (d->result()==QDialog::Accepted) {
            for (int i=0; i<selection.count(); i++) {
                d->getData(selectedModel->beginEditRow(selection[i]));
                selectedModel->endEditRow(selection[i]);
            }
            updateViewMode();
        }
        delete d;
    }
}

void MainWindow::on_btnEdit_clicked()
{
    on_action_Edit_triggered();
}

void MainWindow::rowDoubleClicked(const QModelIndex &)
{
    on_action_Edit_triggered();
}

// Remove
void MainWindow::on_action_Remove_triggered()
{
    if (!checkSelection()) return;
    if (QMessageBox::question(0, S_CONFIRM, tr("Are You really want to delete selected items?"),
            QMessageBox::Yes, QMessageBox::No)==QMessageBox::Yes)
        selectedModel->removeAnyRows(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_btnRemove_clicked()
{
    on_action_Remove_triggered();
}

// Copy
void MainWindow::on_action_Copy_triggered()
{
    if (!checkSelection()) return;
    ContactModel* target = oppositeModel();
    selectedModel->copyRows(selection, target);
    selectedView->clearSelection();
    updateViewMode();
    setButtonsAccess();
    updateHeaders();
}

void MainWindow::on_btnCopy_clicked()
{
    on_action_Copy_triggered();
}

// Move
void MainWindow::on_action_Move_triggered()
{
    if (!checkSelection()) return;
    ContactModel* target = oppositeModel();
    selectedModel->copyRows(selection, target);
    selectedModel->removeAnyRows(selection);
    selectedView->clearSelection();
    updateViewMode();
    setButtonsAccess();
    updateHeaders();
}

void MainWindow::on_btnMove_clicked()
{
    on_action_Move_triggered();
}

// Swap names
void MainWindow::on_action_Swap_names_triggered()
{
    if (!checkSelection()) return;
    selectedModel->swapNames(selection);
    updateViewMode();
    updateHeaders();
}

// Split names
void MainWindow::on_actionS_plit_names_triggered()
{
    if (!checkSelection()) return;
    selectedModel->splitNames(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_btnSwapNames_clicked()
{
    on_action_Swap_names_triggered();
}

// Compare two lists
void MainWindow::on_actionCo_mpare_triggered()
{
    // Clear both selections before compare
    ui->tvLeft->selectionModel()->clearSelection();
    ui->tvRight->selectionModel()->clearSelection();
    // Compare on
    if (selectedModel->viewMode()==ContactModel::Standard) {
        if (!ui->tvRight->isVisible() || oppositeModel()->rowCount()==0 || selectedModel->rowCount()==0) {
            QMessageBox::critical(0, S_ERROR,
                tr("Compare mode requires show two panels and load contact lists in both panels"));
            return;
        }
        selectedModel->setViewMode(ContactModel::CompareMain, oppositeModel());
    }
    // Compare off
    else
        selectedModel->setViewMode(ContactModel::Standard, oppositeModel());
    updateModeStatus();
}

void MainWindow::on_btnCompare_clicked()
{
    on_actionCo_mpare_triggered();
}

// Sort List
void MainWindow::on_action_Sort_toggled(bool needSort)
{
    setSorting(needSort);
    setDlg->setSortingEnabled(needSort);
    updateModeStatus();
}

void MainWindow::on_btnSort_clicked()
{
    ui->action_Sort->toggle();
}

void MainWindow::selectView(QTableView* view)
{
    selectedView = view;
    bool isLeft = selectedView==ui->tvLeft;
    selectedModel = (isLeft ? modLeft : modRight);
    selectedHeader = (isLeft ? ui->lbLeft : ui->lbRight);
}

bool MainWindow::checkSelection(bool errorIfNoSelected, bool onlyOneRowAllowed)
{
    QModelIndexList proxySelection = selectedView->selectionModel()->selectedRows();
    if (proxySelection.count()==0) {
        if (errorIfNoSelected)
            QMessageBox::critical(0, S_ERROR, tr("Record not selected"));
        return false;
    }
    if (onlyOneRowAllowed && (proxySelection.count()>1)) {
        QMessageBox::critical(0, S_ERROR, tr("Group editing not impemented, select one record"));
        return false;
    }
    // If proxy models works...
    ContactSorterFilter* selectedProxy = (selectedView==ui->tvLeft) ? proxyLeft : proxyRight;
    selection.clear();
    foreach(QModelIndex index, proxySelection)
        selection << selectedProxy->mapToSource(index);
    return true;
}

void MainWindow::setSorting(bool needSort)
{
    ui->tvLeft->setSortingEnabled(needSort);
    ui->tvRight->setSortingEnabled(needSort);
    int sortColumn = needSort ? 0 : -1;
    proxyLeft->sort(sortColumn);
    proxyRight->sort(sortColumn);
}

void MainWindow::setLanguage(const QString &language)
{
    QString langCode = "en_GB";
    if (language==QString::fromUtf8("Русский")) // TODO dyn load?
        langCode = "ru_RU";
    QString langPath = qApp->applicationDirPath()+QDir::separator()+QString("doublecontact_%1.qm").arg(langCode);
    QTranslator tr;
    if (!tr.load(langPath))
        QMessageBox::critical(0, S_ERROR, "UI loading error");
    else {
        qApp->installTranslator(&tr);
        ui->retranslateUi(this);
        Phone::standardTypes.fill();
        Email::standardTypes.fill();
        contactColumnHeaders.fill();
        qApp->processEvents();
    }
}

void MainWindow::updateListHeader(ContactModel *model, QLabel *header)
{
    QString sChanged = model->changed() ? "*" : "";
    QString src = model->source();
    if (src.contains(QDir::separator()))
        src = src.mid(src.lastIndexOf(QDir::separator())+1);
    header->setText(sChanged+src);
}

void MainWindow::setButtonsAccess()
{
    bool hasSelectedRows = selectedView->selectionModel()->selectedRows().count();
    bool twoPanels = ui->tvRight->isVisible();
    ui->action_Copy->setEnabled(hasSelectedRows && twoPanels);
    ui->action_Move->setEnabled(hasSelectedRows && twoPanels);
    ui->actionCo_mpare->setEnabled(twoPanels);
    ui->action_Edit->setEnabled(hasSelectedRows);
    ui->action_Remove->setEnabled(hasSelectedRows);
    ui->action_Swap_names->setEnabled(hasSelectedRows);
    ui->btnCopy->setEnabled(hasSelectedRows && twoPanels);
    ui->btnMove->setEnabled(hasSelectedRows && twoPanels);
    ui->btnCompare->setEnabled(hasSelectedRows && twoPanels);
    ui->btnEdit->setEnabled(hasSelectedRows);
    ui->btnRemove->setEnabled(hasSelectedRows);
    ui->btnSwapNames->setEnabled(hasSelectedRows);
}

void MainWindow::selectionChanged()
{
    static bool lockSelection = false;
    // For compare mode, select pair item(s)
    ContactModel::ContactViewMode viewMode = selectedModel->viewMode();
    if (!lockSelection)
    if (viewMode==ContactModel::CompareMain || viewMode==ContactModel::CompareOpposite) {
        if (!checkSelection(false)) return;
        QTableView* oppView = (selectedView==ui->tvLeft) ? ui->tvRight : ui->tvLeft;
        QSortFilterProxyModel* oppProxy = dynamic_cast<QSortFilterProxyModel*>(oppView->model());
        oppView->selectionModel()->clearSelection();
        foreach (const QModelIndex& index, selection) {
            ContactItem& item = selectedModel->itemList()[index.row()];
            if (item.pairItem) {
                QModelIndex oppIndexFirst = oppProxy->mapFromSource(
                            oppositeModel()->index(item.pairIndex, 0));
                QModelIndex oppIndexLast = oppProxy->mapFromSource(
                            oppositeModel()->index(item.pairIndex, selectedModel->columnCount()-1));
                lockSelection = true;
                oppView->selectionModel()->select(QItemSelection(oppIndexFirst, oppIndexLast), QItemSelectionModel::Select);
                lockSelection = false;
            }
        }
    }
    setButtonsAccess();
}

void MainWindow::on_tvLeft_clicked(const QModelIndex&)
{
    setButtonsAccess();
}

void MainWindow::on_tvRight_clicked(const QModelIndex&)
{
    setButtonsAccess();
}

void MainWindow::updateHeaders()
{    
    updateListHeader(modLeft, ui->lbLeft);
    updateListHeader(modRight, ui->lbRight);
    setWindowTitle(selectedModel->source().isEmpty() ?
        tr("Double Contact") :
                       tr("Double Contact - %1").arg(selectedHeader->text()));
}

void MainWindow::updateModeStatus()
{
    QString sm = tr("Mode: ");
    sm += (setDlg->showTwoPanels() ? tr("two panels") : tr("one panel")) + ", ";
    sm += (ui->action_Sort->isChecked() ? tr("sorted") : tr("not sorted")) + ", ";
    switch (modLeft->viewMode()) {
    case ContactModel::Standard:
        sm += tr("simple editing");
        break;
    case ContactModel::CompareMain:
    case ContactModel::CompareOpposite:
        sm += tr("compare");
        break;
    default:
        // TODO duplicate search
        break;
    }
    lbMode->setText(sm);
}

void MainWindow::updateViewMode()
{
    selectedModel->setViewMode(selectedModel->viewMode(), oppositeModel());
}

void MainWindow::setSelectionModelEvents()
{
    connect(ui->tvLeft->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(setButtonsAccess()));
    connect(ui->tvRight->selectionModel(), SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(setButtonsAccess()));
    connect(ui->tvLeft->selectionModel(), SIGNAL(selectionChanged( const QItemSelection&, const QItemSelection&)),
            this, SLOT(selectionChanged()));
    connect(ui->tvRight->selectionModel(), SIGNAL(selectionChanged( const QItemSelection&, const QItemSelection&)),
            this, SLOT(selectionChanged()));
}

ContactModel* MainWindow::oppositeModel()
{
    return (selectedView==ui->tvLeft) ? modRight : modLeft;
}

void MainWindow::askSaveChanges(QCloseEvent *event, ContactModel *model)
{
    if (!model->changed())
        return;
    int res = QMessageBox::question(0, S_CONFIRM,
            tr("'%1' was changed.\nSave changes?").arg(model->source()),
            QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
    switch (res) {
    case QMessageBox::Yes:
        if (model->saveAs(model->source()))
            event->accept();
        else
            event->ignore();
        break;
    case QMessageBox::No:
        event->accept();
        break;
    case QMessageBox::Cancel:
        event->ignore();
        break;
    }
}

void MainWindow::updateConfig()
{
    modLeft->setVisibleColumns(setDlg->columnNames());
    if (modRight)
        modRight->setVisibleColumns(setDlg->columnNames());
    // Language
    setDlg->writeConfig();
    if (setDlg->langChanged())
        setLanguage(setDlg->lang());
}

void MainWindow::on_action_Other_panel_triggered()
{
    if (selectedView==ui->tvLeft)
        selectView(ui->tvRight);
    else
        selectView(ui->tvLeft);
    selectedView->setFocus();
}

void MainWindow::on_actionSettings_triggered()
{
    setDlg->exec();
    if (setDlg->result()==QDialog::Accepted)
        updateConfig();
}

void MainWindow::on_action_Close_triggered()
{
    // TODO ask if model has unsaved changes (after save implementation)
    selectedModel->close();
    updateHeaders();
}

void MainWindow::on_leFilterLeft_textChanged(const QString &arg1)
{
    if (arg1.isEmpty())
        proxyLeft->setFilterWildcard("");
    else
        proxyLeft->setFilterWildcard(arg1);
}

void MainWindow::on_leFilterRight_textChanged(const QString &arg1)
{
    if (arg1.isEmpty())
        proxyRight->setFilterWildcard("");
    else
        proxyRight->setFilterWildcard(arg1);
}

void MainWindow::on_action_Filter_triggered()
{
    if (selectedView==ui->tvLeft)
        ui->leFilterLeft->setFocus();
    else
        ui->leFilterRight->setFocus();
}

void MainWindow::on_actionCompare_Result_triggered()
{
    // TODO check two panels and compare mode
    if ((!ui->tvRight->isVisible())
            || (selectedModel->viewMode()!=ContactModel::CompareMain
            && selectedModel->viewMode()!=ContactModel::CompareOpposite)) {
        QMessageBox::critical(0, S_ERROR, tr("Two panels and compare mode needed for this operation"));
        return;
    }
    bool wasRight = (selectedModel==modRight);
    if (wasRight)
            on_action_Other_panel_triggered(); // to left
    if (!checkSelection(true, true)) return;
    QModelIndex leftSelection = selection[0];
    ContactItem& left = modLeft->beginEditRow(leftSelection);
    on_action_Other_panel_triggered(); // to right
    if (!checkSelection(true, true)) return;
    ContactItem& right = modRight->beginEditRow(selection[0]);
    CompareDialog* d = new CompareDialog(0);
    d->setHeaders(tr("Left item"), tr("Right item"));
    d->setData(left, right);
    d->exec();
    if (d->result()==QDialog::Accepted)
        d->getData(left, right);
        modLeft->endEditRow(leftSelection);
        modRight->endEditRow(selection[0]);
    delete d;
    if (!wasRight)
        on_action_Other_panel_triggered(); // to left
}

void MainWindow::on_action_Drop_slashes_triggered()
{
    if (!checkSelection()) return;
    selectedModel->dropSlashes(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_action_Join_triggered()
{
    if (!checkSelection()) return;
    // Operation required strongly 2 records
    if (selection.count()!=2) {
        QMessageBox::critical(0, S_ERROR, tr("Strongly two records on current panel must be selected for this operation"));
        return;
    }
    ContactItem& i1 = selectedModel->beginEditRow(selection[0]);
    ContactItem& i2 = selectedModel->beginEditRow(selection[1]);
    CompareDialog* d = new CompareDialog(0);
    d->setHeaders(tr("Item 1"), tr("Item 2"));
    d->setData(i1, i2);
    d->exec();
    if (d->result()==QDialog::Accepted) {
        d->getData(i1, i2);
        selectedModel->endEditRow(selection[0]);
        selectedModel->endEditRow(selection[1]);
        updateViewMode();
        updateHeaders();
    }
    delete d;
}

void MainWindow::on_actionSp_lit_triggered()
{
    if (!checkSelection()) return;
    selectedModel->splitNumbers(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_action_Generate_full_name_triggered()
{
    if (!checkSelection()) return;
    selectedModel->generateFullNames(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_actionDrop_full_name_triggered()
{
    if (!checkSelection()) return;
    selectedModel->dropFullNames(selection);
    updateViewMode();
    updateHeaders();
}


void MainWindow::on_actionIntl_phone_prefix_triggered()
{
    if (!checkSelection()) return;
    selectedModel->intlPhonePrefix(selection);
    updateViewMode();
    updateHeaders();
}

void MainWindow::on_actionS_wap_Panels_triggered()
{
    if (!ui->tvRight->isVisible()) {
        QMessageBox::critical(0, S_ERROR,
            tr("Operation requires show two panels"));
        return;
    }
    ContactModel* bufModel = modLeft;
    modLeft = modRight;
    modRight = bufModel;
    ContactSorterFilter * bufProxy = proxyLeft;
    proxyLeft = proxyRight;
    proxyRight = bufProxy;
    ui->tvLeft->setModel(proxyLeft);
    ui->tvRight->setModel(proxyRight);
    updateHeaders();
    setSelectionModelEvents();
}
