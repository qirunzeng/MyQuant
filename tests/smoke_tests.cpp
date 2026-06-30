#include "../src/EtfRotationController.h"
#include "../src/NotesController.h"
#include "../src/SettingsController.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cassert>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    assert(dir.isValid());
    qputenv("MYQUANT_HOME", dir.path().toUtf8());

    SettingsController settings;
    assert(settings.save());

    NotesController notes;
    QVariantMap draft = notes.createFromTemplate("交易复盘");
    draft["title"] = "Smoke Note";
    assert(notes.saveNote(draft));
    assert(!notes.notes().isEmpty());

    EtfRotationController etf(&settings);
    assert(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));
    assert(!etf.equity().isEmpty());
    assert(!etf.rankings().isEmpty());
    assert(!etf.summary().isEmpty());
    assert(etf.dataIssues().isEmpty());
    assert(!etf.advice().isEmpty());
    assert(!etf.pricePoints().isEmpty());
    assert(!etf.tradeCharts().isEmpty());
    const QString firstActualStart = etf.metrics().value("startDate").toString();
    assert(etf.runDefault(false, "20230101", "20231231", 4, 0.25, 0.07, 40000));
    assert(etf.metrics().value("startDate").toString() != firstActualStart);
    assert(etf.metrics().value("startDate").toString() >= QString("2023-01-01"));
    assert(etf.runDefault(false, "20990101", "20991231", 4, 0.25, 0.07, 40000) == false);
    assert(etf.equity().isEmpty());
    assert(etf.metrics().isEmpty());
    assert(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));

    QVariantMap portfolio = etf.portfolio();
    QVariantList positions = portfolio.value("positions").toList();
    if (!positions.isEmpty()) {
        QVariantMap first = positions.first().toMap();
        const QString disabledCode = first.value("code").toString();
        first["enabled"] = false;
        positions[0] = first;
        assert(etf.savePortfolio(portfolio.value("availableCash").toDouble(), positions));
        assert(etf.runDefault(false, "20220101", "20231231", 4, 0.25, 0.07, 40000));
        for (const QVariant& value : etf.rankings())
            assert(value.toMap().value("symbol").toString() != disabledCode);
    }
    portfolio = etf.portfolio();
    positions = portfolio.value("positions").toList();
    if (positions.size() > 1) {
        const QString removedCode = positions.last().toMap().value("code").toString();
        positions.removeLast();
        assert(etf.savePortfolio(portfolio.value("availableCash").toDouble(), positions));
        etf.refreshPortfolio();
        bool foundRemoved = false;
        for (const QVariant& value : etf.portfolio().value("positions").toList()) {
            if (value.toMap().value("code").toString() == removedCode)
                foundRemoved = true;
        }
        assert(!foundRemoved);
    }

    return 0;
}
