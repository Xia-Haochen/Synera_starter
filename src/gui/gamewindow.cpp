#include "gamewindow.h"
#include "core/game.h"
#include "core/shop.h"
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QMessageBox>
#include <QApplication>
#include <QFileDialog>

GameWindow::GameWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_game(new Game(this))
{
    setupUI();
    connect(m_game, &Game::phaseChanged, this, &GameWindow::updateUIState);
    connect(m_game, &Game::stateUpdated, this, &GameWindow::updateUIState);
    connect(m_game, &Game::gameEnded, this, &GameWindow::onGameEnded);
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
        m_saveButton->setEnabled(true);
    } else if (m_game->getPhase() == Phase::Combat) {
        phaseStr = "Phase: Combat";
        m_startBattleButton->setEnabled(false);
        m_resetButton->setEnabled(false);
        m_saveButton->setEnabled(false);
    } else {
        phaseStr = "Phase: Resolve";
        m_startBattleButton->setEnabled(false);
        m_resetButton->setEnabled(false);
        m_saveButton->setEnabled(false);
    }
    m_phaseLabel->setText(phaseStr);

    updateShopUI();
    m_refreshButton->setEnabled(m_game->getPhase() == Phase::Prep
        && m_game->getPlayerState().gold >= Shop::REFRESH_COST);
}

void GameWindow::setupUI()
{
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);

    // Style
    setStyleSheet(R"(
        QMainWindow { background-color: #2b2b2b; }
        QWidget { background-color: #2b2b2b; color: #f2f2f2; }
        QPushButton { background-color: #2f2f2f; color: #f2f2f2; border: 1px solid #565656; border-radius: 4px; padding: 6px 14px; font-size: 13px; }
        QPushButton:hover { background-color: #3a3a3a; }
        QPushButton:pressed { background-color: #242424; }
    )");

    // --- Main Menu ---
    m_mainMenuWidget = new QWidget(this);
    QVBoxLayout* menuLayout = new QVBoxLayout(m_mainMenuWidget);
    menuLayout->setAlignment(Qt::AlignCenter);

    QLabel* titleLabel = new QLabel("Synera Starter", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    menuLayout->addWidget(titleLabel);

    QPushButton* newGameBtn = new QPushButton("新游戏", this);
    QPushButton* loadGameBtn = new QPushButton("加载存档", this);
    menuLayout->addWidget(newGameBtn);
    menuLayout->addWidget(loadGameBtn);

    connect(newGameBtn, &QPushButton::clicked, this, &GameWindow::onNewGameClicked);
    connect(loadGameBtn, &QPushButton::clicked, this, &GameWindow::onLoadGameClicked);

    // --- Game Widget ---
    m_gameWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_gameWidget);
    
    m_phaseLabel = new QLabel(this);
    m_view = new QGraphicsView(this);
    m_resetButton = new QPushButton("Reset", this);
    m_startBattleButton = new QPushButton("Start Battle", this);
    m_saveButton = new QPushButton("Save Game", this);
    m_statsLabel = new QLabel(this);

    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::NoDrag);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setMouseTracking(true);
    m_view->viewport()->setMouseTracking(true);

    m_phaseLabel->setAlignment(Qt::AlignCenter);
    QFont phaseFont = m_phaseLabel->font();
    phaseFont.setPointSize(16);
    phaseFont.setBold(true);
    m_phaseLabel->setFont(phaseFont);
    m_mainLayout->addWidget(m_phaseLabel, 0);

    m_mainLayout->addWidget(m_view, 1);

    m_shopPanel = new QWidget(this);
    QHBoxLayout* shopLayout = new QHBoxLayout(m_shopPanel);
    shopLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* shopLabel = new QLabel("商店", this);
    QFont shopFont = shopLabel->font();
    shopFont.setPointSize(12);
    shopFont.setBold(true);
    shopLabel->setFont(shopFont);
    shopLayout->addWidget(shopLabel);

    for (int i = 0; i < 5; ++i) {
        m_shopSlots[i] = new QPushButton(this);
        m_shopSlots[i]->setFixedSize(100, 40);
        shopLayout->addWidget(m_shopSlots[i]);
    }
    m_refreshButton = new QPushButton("刷新(3g)", this);
    m_refreshButton->setFixedSize(90, 40);
    shopLayout->addWidget(m_refreshButton);
    m_mainLayout->addWidget(m_shopPanel);

    QWidget* controlBar = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->addWidget(m_statsLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(m_saveButton);
    controlLayout->addWidget(m_resetButton);
    controlLayout->addWidget(m_startBattleButton);
    m_mainLayout->addWidget(controlBar);

    connect(m_resetButton, &QPushButton::clicked, this, &GameWindow::onResetButtonClicked);
    connect(m_startBattleButton, &QPushButton::clicked, this, &GameWindow::onStartBattleClicked);
    connect(m_saveButton, &QPushButton::clicked, this, &GameWindow::onSaveGameClicked);

    connect(m_shopSlots[0], &QPushButton::clicked, this, &GameWindow::onBuySlot0);
    connect(m_shopSlots[1], &QPushButton::clicked, this, &GameWindow::onBuySlot1);
    connect(m_shopSlots[2], &QPushButton::clicked, this, &GameWindow::onBuySlot2);
    connect(m_shopSlots[3], &QPushButton::clicked, this, &GameWindow::onBuySlot3);
    connect(m_shopSlots[4], &QPushButton::clicked, this, &GameWindow::onBuySlot4);
    connect(m_refreshButton, &QPushButton::clicked, this, &GameWindow::onRefreshShop);
    connect(&m_game->getShop(), &Shop::shopChanged, this, &GameWindow::updateShopUI);

    m_view->setScene(m_game->scene());

    m_stackedWidget->addWidget(m_mainMenuWidget);
    m_stackedWidget->addWidget(m_gameWidget);
    m_stackedWidget->setCurrentWidget(m_mainMenuWidget);
}

void GameWindow::updateShopUI()
{
    if (!m_game) return;
    for (int i = 0; i < 5; ++i) {
        if (m_game->getShop().isSlotEmpty(i)) {
            m_shopSlots[i]->setText("已售");
            m_shopSlots[i]->setEnabled(false);
        } else {
            JobType job = m_game->getShop().getSlot(i);
            QString name;
            switch (job) {
                case JobType::Warrior: name = "战士"; break;
                case JobType::Mage:    name = "法师"; break;
                case JobType::Archer:  name = "弓手"; break;
            }
            m_shopSlots[i]->setText(QString("%1(10g)").arg(name));
            m_shopSlots[i]->setEnabled(m_game->getPhase() == Phase::Prep
                                       && m_game->getPlayerState().gold >= Shop::UNIT_COST);
        }
    }
}

void GameWindow::onBuySlot0() { if (m_game) m_game->buyFromShopSlot(0); }
void GameWindow::onBuySlot1() { if (m_game) m_game->buyFromShopSlot(1); }
void GameWindow::onBuySlot2() { if (m_game) m_game->buyFromShopSlot(2); }
void GameWindow::onBuySlot3() { if (m_game) m_game->buyFromShopSlot(3); }
void GameWindow::onBuySlot4() { if (m_game) m_game->buyFromShopSlot(4); }
void GameWindow::onRefreshShop() { if (m_game) m_game->rollShop(); }

void GameWindow::onNewGameClicked() {
    m_game->initialize();
    updateUIState();
    m_stackedWidget->setCurrentWidget(m_gameWidget);
}

void GameWindow::onLoadGameClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "加载存档", "", "Save Files (*.json)");
    if (!fileName.isEmpty()) {
        if (m_game->loadFromFile(fileName)) {
            updateUIState();
            m_stackedWidget->setCurrentWidget(m_gameWidget);
        } else {
            QMessageBox::warning(this, "错误", "加载存档失败！");
        }
    }
}

void GameWindow::onSaveGameClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "保存进度", "save.json", "Save Files (*.json)");
    if (!fileName.isEmpty()) {
        if (m_game->saveToFile(fileName)) {
            QMessageBox::information(this, "成功", "保存成功，游戏将退出！");
            QApplication::quit();
        } else {
            QMessageBox::warning(this, "错误", "保存失败！");
        }
    }
}

void GameWindow::onGameEnded(bool isVictory) {
    QString title = isVictory ? "游戏胜利" : "游戏失败";
    QString msg = isVictory ? "恭喜你战胜了所有敌人！" : "你倒下了...";
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    QPushButton* restartBtn = msgBox.addButton("重新开始", QMessageBox::AcceptRole);
    QPushButton* exitBtn = msgBox.addButton("退出游戏", QMessageBox::RejectRole);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == restartBtn) {
        m_game->initialize();
        updateUIState();
    } else if (msgBox.clickedButton() == exitBtn) {
        QApplication::quit();
    }
}

