#include "controlwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>

// ── 配色 ────────────────────────────────────────────────────────
const QString ControlWidget::C_BG     = QStringLiteral("#111318");
const QString ControlWidget::C_CARD   = QStringLiteral("#181b22");
const QString ControlWidget::C_BTN    = QStringLiteral("#1e2433");
const QString ControlWidget::C_ACCENT = QStringLiteral("#5b8def");
const QString ControlWidget::C_TXT    = QStringLiteral("#d0d4dc");

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void ControlWidget::setupUi()
{
    setStyleSheet(QStringLiteral("background:transparent;"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(6);

    // ── 自定义命令输入 ──
    auto *customRow = new QHBoxLayout;
    customRow->setSpacing(6);

    auto *customLbl = new QLabel(QStringLiteral("命令: 0x"));
    customLbl->setStyleSheet(QString("font-size:12px;color:#6b7280;background:transparent;"));
    customRow->addWidget(customLbl);

    m_customCmdEdit = new QLineEdit;
    m_customCmdEdit->setPlaceholderText(QStringLiteral("00~FF"));
    m_customCmdEdit->setMaxLength(2);
    m_customCmdEdit->setFixedWidth(56);
    m_customCmdEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{0,2}$")), m_customCmdEdit));
    m_customCmdEdit->setStyleSheet(QString(
        "QLineEdit{font-size:14px;color:%1;background:%2;border:1px solid #2a3145;"
        "border-radius:6px;padding:6px 10px;}"
        "QLineEdit:focus{border-color:%3;}")
        .arg(C_TXT, C_BTN, C_ACCENT));
    customRow->addWidget(m_customCmdEdit);

    m_customSendBtn = new QPushButton(QStringLiteral("发送  4字节帧: A5 CMD CMD 5A"));
    m_customSendBtn->setMinimumHeight(38);
    m_customSendBtn->setStyleSheet(QString(
        "QPushButton{border:none;border-radius:8px;padding:8px 18px;"
        "font-size:12px;font-weight:bold;color:#fff;background:%1;}"
        "QPushButton:hover{background:%2;}")
        .arg(C_ACCENT, QStringLiteral("#4a7de0")));
    connect(m_customSendBtn, &QPushButton::clicked,
            this, &ControlWidget::onCustomSend);
    connect(m_customCmdEdit, &QLineEdit::returnPressed,
            this, &ControlWidget::onCustomSend);

    customRow->addWidget(m_customSendBtn);
    customRow->addStretch();
    mainLayout->addLayout(customRow);
}

void ControlWidget::onCustomSend()
{
    QString text = m_customCmdEdit->text().trimmed();
    if (text.isEmpty()) return;

    bool ok;
    uint8_t cmd = static_cast<uint8_t>(text.toUInt(&ok, 16));
    if (!ok) {
        QMessageBox::warning(this,
            QStringLiteral("输入错误"),
            QStringLiteral("请输入有效的十六进制值 (00~FF)"));
        return;
    }

    emit commandRequested(cmd);
    m_customCmdEdit->clear();
}
