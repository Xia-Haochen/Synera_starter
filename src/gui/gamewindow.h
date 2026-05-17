#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>

class Game;
class QGraphicsView;
class QPushButton;
class QVBoxLayout;

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

private:
    void setupUI();

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QGraphicsView* m_view;
    QPushButton* m_resetButton;
    QPushButton* m_startBattleButton;
    class QLabel* m_statsLabel;
    class QLabel* m_phaseLabel; // 显示当前阶段的标签
    Game* m_game;
};

#endif // GAMEWINDOW_H
