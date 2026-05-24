#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>

class Game;
class QGraphicsView;
class QPushButton;
class QVBoxLayout;
class QStackedWidget;

// GameWindow 是主界面容器：
// - 承载 QGraphicsView 展示战场
// - 提供基础控制按钮（当前为 Reset）
class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget* parent = nullptr);
    ~GameWindow();

private slots:
    void onResetButtonClicked();
    void onStartBattleClicked();
    void updateUIState();
    void updateShopUI();
    void onBuySlot0();
    void onBuySlot1();
    void onBuySlot2();
    void onBuySlot3();
    void onBuySlot4();
    void onBuyBoardCap();
    void onRefreshShop();
    void onNewGameClicked();
    void onLoadGameClicked();
    void onSaveGameClicked();
    void onGameEnded(bool isVictory);

private:
    void setupUI();

    QStackedWidget* m_stackedWidget;
    QWidget* m_mainMenuWidget;
    QWidget* m_gameWidget;

    QVBoxLayout* m_mainLayout;
    QGraphicsView* m_view;
    QPushButton* m_resetButton;
    QPushButton* m_startBattleButton;
    QPushButton* m_saveButton;
    class QLabel* m_statsLabel;
    class QLabel* m_phaseLabel; // 显示当前阶段的标签
    Game* m_game;

    // 商店 UI
    QWidget* m_shopPanel;
    QPushButton* m_boardCapButton; // 人口上限购买按钮
    QPushButton* m_shopSlots[5];
    QPushButton* m_refreshButton;
};

#endif // GAMEWINDOW_H
