#include "gamewindow.h"
#include "core/game.h"
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

GameWindow::GameWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_centralWidget(new QWidget(this))
    , m_mainLayout(new QVBoxLayout())
    , m_view(new QGraphicsView(this))
    , m_resetButton(new QPushButton("Reset", this))
    , m_startBattleButton(new QPushButton("Start Battle", this))
    , m_statsLabel(new QLabel(this))
    , m_phaseLabel(new QLabel(this))
    , m_game(new Game(this))
{
    // UI 与逻辑层初始化顺序：先搭界面，再初始化游戏内容。
    setupUI();
    m_game->initialize();
    updateUIState();

    connect(m_game, &Game::phaseChanged, this, &GameWindow::updateUIState);
    connect(m_game, &Game::stateUpdated, this, &GameWindow::updateUIState);
}

GameWindow::~GameWindow() = default;

void GameWindow::onResetButtonClicked()
{
    if (m_game && m_game->getPhase() == Phase::Prep) {
        m_game->reset();
    }
}

void GameWindow::onStartBattleClicked()
{
    if (m_game && m_game->getPhase() == Phase::Prep) {
        m_game->advancePhase();
    }
}

void GameWindow::updateUIState()
{
    if (!m_game) return;

    auto state = m_game->getPlayerState();
    m_statsLabel->setText(QString("HP: %1 | Gold: %2 | Round: %3")
                          .arg(state.hp).arg(state.gold).arg(state.round));

    QString phaseStr;
    if (m_game->getPhase() == Phase::Prep) {
        phaseStr = "Phase: Preparation";
        m_startBattleButton->setEnabled(true);
        m_resetButton->setEnabled(true);
    } else if (m_game->getPhase() == Phase::Combat) {
        phaseStr = "Phase: Combat";
        m_startBattleButton->setEnabled(false);
        m_resetButton->setEnabled(false);
    } else {
        phaseStr = "Phase: Resolve";
        m_startBattleButton->setEnabled(false);
        m_resetButton->setEnabled(false);
    }
    m_phaseLabel->setText(phaseStr);
}

void GameWindow::setupUI()
{
    // 设置根布局与统一深色主题。
    setCentralWidget(m_centralWidget);
    m_centralWidget->setLayout(m_mainLayout);

    setStyleSheet(R"(
        QMainWindow {
            background-color: #2b2b2b;
        }
        QWidget {
            background-color: #2b2b2b;
            color: #f2f2f2;
        }
        QPushButton {
            background-color: #2f2f2f;
            color: #f2f2f2;
            border: 1px solid #565656;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #3a3a3a;
        }
        QPushButton:pressed {
            background-color: #242424;
        }
    )");

    // 视图参数：关闭滚动条，固定为场景展示窗口。
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);

    // 顶部阶段栏
    m_phaseLabel->setAlignment(Qt::AlignCenter);
    QFont phaseFont = m_phaseLabel->font();
    phaseFont.setPointSize(16);
    phaseFont.setBold(true);
    m_phaseLabel->setFont(phaseFont);
    m_mainLayout->addWidget(m_phaseLabel, 0);

    m_mainLayout->addWidget(m_view, 1);

    // 底部控制栏：当前仅保留 Reset，可按需扩展商店/回合按钮等。
    QWidget* controlBar = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_statsLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(m_resetButton);
    controlLayout->addWidget(m_startBattleButton);
    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked,
            this, &GameWindow::onResetButtonClicked);
    connect(m_startBattleButton, &QPushButton::clicked,
            this, &GameWindow::onStartBattleClicked);

    // 将逻辑层场景挂载到视图。
    m_view->setScene(m_game->scene());
}
