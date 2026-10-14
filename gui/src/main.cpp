#include "MainWindow.hpp"
#include "DataMaster.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDialog>
#include <QFont>
#include <QTimer>

// 使い方:
//   monopoly_gui.exe [ログファイル]
//   monopoly_gui.exe --open game.log --goto 120 --select P-BR1 --screenshot out.png   (画面確認用)
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("MonopolyRecorder"));
    QApplication::setApplicationName(QStringLiteral("MonopolyRecorder"));
    QApplication::setApplicationDisplayName(QStringLiteral("Business Monopoly Recorder"));
    QApplication::setStyle(QStringLiteral("Fusion"));
    {   // 全体の文字を少し大きく
        QFont f = QApplication::font();
        f.setPointSizeF(f.pointSizeF() * 1.3);
        QApplication::setFont(f);
    }

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption openOpt(QStringLiteral("open"), QStringLiteral("Open a log file"), QStringLiteral("file"));
    QCommandLineOption gotoOpt(QStringLiteral("goto"), QStringLiteral("Jump to action number"), QStringLiteral("n"));
    QCommandLineOption selectOpt(QStringLiteral("select"), QStringLiteral("Select a card"), QStringLiteral("card"));
    QCommandLineOption trayOpt(QStringLiteral("demo-tray"), QStringLiteral("Fill the trade tray with a sample (for screenshots)"));
    QCommandLineOption hoverOpt(QStringLiteral("hover"), QStringLiteral("Show the hover panel of a card (for screenshots)"), QStringLiteral("card"));
    QCommandLineOption scaleOpt(QStringLiteral("text-scale"), QStringLiteral("Text scale for this run"), QStringLiteral("scale"));
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Save a screenshot and quit"), QStringLiteral("png"));
    parser.addOptions({openOpt, gotoOpt, selectOpt, trayOpt, hoverOpt, scaleOpt, shotOpt});
    parser.addPositionalArgument(QStringLiteral("log"), QStringLiteral("Log file to open"));
    parser.process(app);

    const bool shot = parser.isSet(shotOpt);
    QString file = parser.value(openOpt);
    if (file.isEmpty() && !parser.positionalArguments().isEmpty()) file = parser.positionalArguments().first();

    MainWindow w(nullptr, !shot && file.isEmpty());
    if (shot) w.setAutosaveEnabled(false);  // 画面確認では普段の自動保存を上書きしない
    if (!file.isEmpty()) w.loadFile(file);
    if (parser.isSet(gotoOpt)) w.gotoAction(static_cast<size_t>(parser.value(gotoOpt).toInt()));
    if (parser.isSet(selectOpt)) w.selectCardPublic(DataMaster::fromString(parser.value(selectOpt).toStdString()));
    if (parser.isSet(trayOpt)) w.demoTray();
    if (parser.isSet(scaleOpt)) w.setTextScaleForRun(parser.value(scaleOpt).toDouble());
    if (parser.isSet(hoverOpt)) w.demoHover(DataMaster::fromString(parser.value(hoverOpt).toStdString()));
    w.show();

    if (shot) {
        const QString out = parser.value(shotOpt);
        QTimer::singleShot(1200, &w, [&w, out] {
            w.grab().save(out);
            QApplication::quit();
        });
    }
    return app.exec();
}
