// Copyright (C) 2026 Sean Moon
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "LinksWidget.h"

#include <functional>

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

// ── LinksDialog ────────────────────────────────────────────────────────────────
// Modal dialog for adding or editing a bookmark (title + URL).

class LinksDialog : public QDialog {
public:
    LinksDialog(const QString& title, const QString& url, QWidget* parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(title.isEmpty() ? "Add Bookmark" : "Edit Bookmark");
        setMinimumWidth(320);

        auto* form = new QFormLayout(this);
        form->setSpacing(8);
        form->setContentsMargins(16, 16, 16, 16);

        titleEdit_ = new QLineEdit(title, this);
        titleEdit_->setPlaceholderText("e.g. GitHub");
        form->addRow("Title:", titleEdit_);

        urlEdit_ = new QLineEdit(url, this);
        urlEdit_->setPlaceholderText("https://");
        form->addRow("URL:", urlEdit_);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);

        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (titleEdit_->text().trimmed().isEmpty() ||
                urlEdit_->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Validation", "Title and URL must not be empty.");
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    QString title() const { return titleEdit_->text().trimmed(); }
    QString url()   const { return urlEdit_->text().trimmed(); }

private:
    QLineEdit* titleEdit_;
    QLineEdit* urlEdit_;
};

// ── LinksDisplay ───────────────────────────────────────────────────────────────
// The visible widget: header row + bookmark list.
// Communicates changes back to the plugin via callbacks.

class LinksDisplay : public QWidget {
    Q_OBJECT

public:
    using BookmarkAddedCb   = std::function<void(const Bookmark&)>;
    using BookmarkEditedCb  = std::function<void(int, const Bookmark&)>;
    using BookmarkRemovedCb = std::function<void(int)>;

    LinksDisplay(const QList<Bookmark>&  bookmarks,
                 BookmarkAddedCb         onAdded,
                 BookmarkEditedCb        onEdited,
                 BookmarkRemovedCb       onRemoved,
                 QWidget*                parent = nullptr)
        : QWidget(parent)
        , bookmarks_(bookmarks)
        , onAdded_(std::move(onAdded))
        , onEdited_(std::move(onEdited))
        , onRemoved_(std::move(onRemoved)) {
        setupUi();
        populateList();
    }

    const QList<Bookmark>& bookmarks() const { return bookmarks_; }

private:
    void setupUi() {
        setStyleSheet(
            "QWidget { background: transparent; color: #c8cee8; }"
            "QListWidget {"
            "  background: #12121e; border: 1px solid #2a2a45;"
            "  border-radius: 6px; padding: 2px;"
            "}"
            "QListWidget::item {"
            "  padding: 5px 8px; border-radius: 4px;"
            "}"
            "QListWidget::item:hover { background: #1e1e35; }"
            "QListWidget::item:selected { background: #2a3a6a; color: #e0e4ff; }"
            "QPushButton {"
            "  background: transparent; color: #707090;"
            "  border: none; border-radius: 5px; padding: 2px 6px;"
            "}"
            "QPushButton:hover { background: #2d2d4a; color: #c8cee8; }");

        auto* vbox = new QVBoxLayout(this);
        vbox->setContentsMargins(8, 8, 8, 8);
        vbox->setSpacing(6);

        // Header row: title + [+] button
        auto* header = new QHBoxLayout();
        auto* title  = new QLabel("Links", this);
        title->setStyleSheet(
            "QLabel { font-size: 14px; font-weight: 700; color: #e0e4ff; }");
        addBtn_ = new QPushButton("+", this);
        addBtn_->setFixedSize(24, 24);
        addBtn_->setToolTip("Add bookmark");
        addBtn_->setStyleSheet(
            "QPushButton { font-size: 16px; font-weight: bold;"
            "  background: transparent; color: #707090;"
            "  border: none; border-radius: 5px; padding: 0px; }"
            "QPushButton:hover { background: #2d2d4a; color: #c8cee8; }");
        header->addWidget(title);
        header->addStretch();
        header->addWidget(addBtn_);
        vbox->addLayout(header);

        // Bookmark list
        list_ = new QListWidget(this);
        list_->setContextMenuPolicy(Qt::CustomContextMenu);
        vbox->addWidget(list_, 1);

        connect(addBtn_, &QPushButton::clicked,      this, &LinksDisplay::onAdd);
        connect(list_,   &QListWidget::itemClicked,  this, &LinksDisplay::onItemClicked);
        connect(list_,   &QListWidget::itemDoubleClicked, this, &LinksDisplay::onItemDoubleClicked);
        connect(list_,   &QListWidget::customContextMenuRequested,
                this, &LinksDisplay::onContextMenu);
    }

    void populateList() {
        list_->clear();
        for (const auto& bm : bookmarks_) {
            auto* item = new QListWidgetItem(bm.title, list_);
            item->setToolTip(bm.url);
            item->setData(Qt::UserRole, bm.url);
        }
    }

    void onAdd() {
        LinksDialog dlg("", "", this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        Bookmark bm{dlg.title(), dlg.url()};
        bookmarks_.append(bm);
        onAdded_(bm);
        auto* item = new QListWidgetItem(bm.title, list_);
        item->setToolTip(bm.url);
        item->setData(Qt::UserRole, bm.url);
    }

    void onItemClicked(QListWidgetItem* item) {
        if (!item) return;
        QDesktopServices::openUrl(QUrl(item->data(Qt::UserRole).toString()));
    }

    void onItemDoubleClicked(QListWidgetItem* item) {
        if (!item) return;
        const int row = list_->row(item);
        LinksDialog dlg(item->text(), item->data(Qt::UserRole).toString(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        Bookmark bm{dlg.title(), dlg.url()};
        bookmarks_[row] = bm;
        onEdited_(row, bm);
        item->setText(bm.title);
        item->setToolTip(bm.url);
        item->setData(Qt::UserRole, bm.url);
    }

    void onContextMenu(const QPoint& pos) {
        QListWidgetItem* item = list_->itemAt(pos);
        if (!item) return;
        const int row = list_->row(item);
        QMenu menu(this);
        QAction* del = menu.addAction("Delete");
        if (menu.exec(list_->mapToGlobal(pos)) == del) {
            bookmarks_.removeAt(row);
            onRemoved_(row);
            delete list_->takeItem(row);
        }
    }

    QList<Bookmark>  bookmarks_;
    BookmarkAddedCb  onAdded_;
    BookmarkEditedCb onEdited_;
    BookmarkRemovedCb onRemoved_;

    QPushButton*  addBtn_;
    QListWidget*  list_;
};

#include "LinksWidget.moc"

// ── LinksWidget (IWidget plugin) ───────────────────────────────────────────────

LinksWidget::LinksWidget(QObject* parent) : QObject(parent) {}

void LinksWidget::initialize(dashboard::WidgetContext* /*context*/) {}

QWidget* LinksWidget::createWidget(QWidget* parent) {
    display_ = new LinksDisplay(
        bookmarks_,
        [this](const Bookmark& bm) {
            bookmarks_.append(bm);
        },
        [this](int index, const Bookmark& bm) {
            if (index >= 0 && index < bookmarks_.size())
                bookmarks_[index] = bm;
        },
        [this](int index) {
            if (index >= 0 && index < bookmarks_.size())
                bookmarks_.removeAt(index);
        },
        parent);
    connect(display_, &QObject::destroyed, this, [this]() {
        display_ = nullptr;
    });
    return display_;
}

QJsonObject LinksWidget::serialize() const {
    const QList<Bookmark>& snapshot = display_ ? display_->bookmarks() : bookmarks_;
    QJsonArray arr;
    for (const auto& bm : snapshot)
        arr.append(QJsonObject{{"title", bm.title}, {"url", bm.url}});
    return {{"bookmarks", arr}};
}

void LinksWidget::deserialize(const QJsonObject& data) {
    bookmarks_.clear();
    const auto arr = data["bookmarks"].toArray();
    for (const auto& v : arr) {
        const auto obj = v.toObject();
        bookmarks_.append({obj["title"].toString(), obj["url"].toString()});
    }
}

dashboard::WidgetMetadata LinksWidget::metadata() const {
    return {
        .name        = "Links",
        .version     = "1.0.0",
        .author      = "Dashboard",
        .description = "Clickable bookmark list",
        .minSize     = QSize(180, 150),
        .maxSize     = QSize(500, 800),
        .defaultSize = QSize(240, 320),
    };
}
