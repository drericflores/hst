/***********************************************************
 * Author: Dr. Eric O. Flores
 * Date: 2025-09-06
 * Description: A simple hardware stress testing tool using
 *              common Linux command-line utilities.
 * License: MIT
 * **********************************************************/

#include <QtWidgets>
#include <filesystem>
#include <fstream>
#include <optional>
#include <chrono>

// -----------------------------
// App metadata
// -----------------------------
static const char* APP_NAME = "Hardware Stress Testing Tool";
static const char* VERSION  = "2.0";
static const char* REVISION = "2025-09-06";
static const char* AUTHOR   = "Dr. Eric O. Flores";

// -----------------------------
// util
// -----------------------------

static QString logDirPath() {
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QDir d(home + "/HardwareStressTest/logs");
    if (!d.exists()) d.mkpath(".");
    return d.absolutePath();
}

static QString timestamp() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
}

static bool which(const QString& exe, QString* outPath=nullptr) {
    QProcess proc;
    proc.start("bash", {"-lc", "command -v " + exe});
    proc.waitForFinished(1500);
    auto path = QString::fromLocal8Bit(proc.readAllStandardOutput()).trimmed();
    bool ok = !path.isEmpty();
    if (ok && outPath) *outPath = path;
    return ok;
}

// -----------------------------
// DonutGauge (compact semicircle)
// -----------------------------

class DonutGauge : public QWidget {
    Q_OBJECT
public:
    explicit DonutGauge(QWidget* parent=nullptr)
        : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(200, 160);
    }

    QSize sizeHint() const override { return {200,160}; }

    void setLabel(const QString& t) { m_label = t; update(); }
    void setArcColor(const QColor& c) { m_arc = c; update(); }
    void setTrackColor(const QColor& c) { m_track = c; update(); }
    void setTextColor(const QColor& c) { m_text = c; update(); }
    void setCaptionColor(const QColor& c) { m_caption = c; update(); }
    void setCaptionColorCoded(bool on) { m_captionColorCoded = on; update(); }

    void setValue(double v, const QString& cap) {
        m_value = std::clamp(v, 0.0, 1.0);
        m_captionText = cap;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int pad = 10;
        const int labelBand = 22;
        const int arcw = 14;

        QRectF arcRect(pad, pad + labelBand,
                       width() - 2*pad,
                       std::min(height()*2 - 2*pad, width() - 2*pad));

        // Label (above arc)
        p.setPen(m_text);
        QFont f = font(); f.setBold(true); f.setPointSize(10);
        p.setFont(f);
        p.drawText(QRect(0, pad, width(), 16), Qt::AlignHCenter|Qt::AlignVCenter, m_label);

        // Track
        QPen trackPen(m_track, arcw);
        trackPen.setCapStyle(Qt::RoundCap);
        p.setPen(trackPen);
        // use 280 degrees sweep starting at 280
        p.drawArc(arcRect, 16*280, 16*280);

        // Value arc
        QPen arcPen(m_arc, arcw);
        arcPen.setCapStyle(Qt::RoundCap);
        p.setPen(arcPen);
        int extent = int(280.0 * m_value);
        p.drawArc(arcRect, 16*280, 16*extent);

        // Center percent
        QFont fv = font(); fv.setBold(true); fv.setPointSize(12);
        p.setFont(fv);
        p.setPen(m_text);
        QString pct = QString::number(int(std::round(m_value*100))) + "%";
        QPoint center(width()/2, int(arcRect.top() + arcRect.height()*0.55));
        p.drawText(QRect(center.x()-60, center.y()-12, 120, 24), Qt::AlignCenter, pct);

        // Caption
        p.setFont(font());
        p.setPen(m_captionColorCoded ? m_arc : m_caption);
        p.drawText(QRect(0, height()-18, width(), 16), Qt::AlignHCenter|Qt::AlignVCenter, m_captionText);
    }

private:
    QString m_label, m_captionText;
    QColor  m_arc  = QColor("#84cc16");
    QColor  m_track= QColor("#c7ced6");
    QColor  m_text = Qt::black;
    QColor  m_caption = Qt::black;
    bool    m_captionColorCoded = false;
    double  m_value = 0.0;
};

// -----------------------------
// Lightweight system monitor (Linux)
// -----------------------------

struct CpuSnapshot {
    quint64 user=0,nice=0,sys=0,idle=0,iowait=0,irq=0,softirq=0,steal=0,guest=0,guest_nice=0;
};
static std::optional<CpuSnapshot> readCpu() {
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return std::nullopt;
    auto line = f.readLine();
    QTextStream ts(line);
    QString cpu; ts >> cpu; if (cpu != "cpu") return std::nullopt;
    CpuSnapshot s;
    ts >> s.user >> s.nice >> s.sys >> s.idle >> s.iowait >> s.irq >> s.softirq >> s.steal >> s.guest >> s.guest_nice;
    return s;
}
static double cpuPercent() {
    static auto prev = readCpu();
    auto now = readCpu();
    if (!prev || !now) return 0.0;
    auto deltaIdle = (now->idle + now->iowait) - (prev->idle + prev->iowait);
    auto prevNon = (prev->user+prev->nice+prev->sys+prev->irq+prev->softirq+prev->steal);
    auto nowNon  = (now->user +now->nice +now->sys +now->irq +now->softirq +now->steal);
    auto deltaNon = nowNon - prevNon;
    auto total = deltaIdle + deltaNon;
    prev = now;
    if (total <= 0) return 0.0;
    return (double(deltaNon) / double(total)) * 100.0;
}

static double memPercent(double* usedGiB=nullptr, double* totalGiB=nullptr) {
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly|QIODevice::Text)) return 0.0;
    QHash<QString,qulonglong> map;
    while (!f.atEnd()) {
        auto line = f.readLine();
        auto parts = QString(line).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            bool ok=false;
            qulonglong kB = parts[1].toULongLong(&ok);
            if (ok) map[parts[0].remove(':')] = kB; // store in kB
        }
    }
    qulonglong MemTotal = map.value("MemTotal",0);
    qulonglong MemFree  = map.value("MemFree",0);
    qulonglong Buffers  = map.value("Buffers",0);
    qulonglong Cached   = map.value("Cached",0);
    qulonglong SReclaim = map.value("SReclaimable",0);
    qulonglong Shmem    = map.value("Shmem",0);

    qulonglong avail_kB = map.value("MemAvailable",0);
    double used_kB = (MemTotal - avail_kB);
    double pct = (MemTotal == 0) ? 0.0 : (used_kB / double(MemTotal)) * 100.0;

    if (usedGiB)  *usedGiB = used_kB/1024.0/1024.0;
    if (totalGiB) *totalGiB = MemTotal/1024.0/1024.0;
    return pct;
}

static double rootDiskPercent(double* usedGiB=nullptr, double* totalGiB=nullptr) {
    struct statvfs s{};
    if (statvfs("/", &s) != 0) return 0.0;
    unsigned long long total = s.f_blocks * (unsigned long long)s.f_frsize;
    unsigned long long avail = s.f_bavail * (unsigned long long)s.f_frsize;
    unsigned long long used  = total - avail;
    if (total == 0) return 0.0;
    if (usedGiB)  *usedGiB  = used / (1024.0*1024.0*1024.0);
    if (totalGiB) *totalGiB = total / (1024.0*1024.0*1024.0);
    return (double)used / (double)total * 100.0;
}

// -----------------------------
// Main Window
// -----------------------------

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow() {
        setWindowTitle(QString("%1").arg(APP_NAME));
        resize(900, 620);
        setMinimumSize(820, 540);

        buildUi();
        connectSignals();
        applyLightTheme();
        monitorTimer.start(1000);
    }

private:
    // --- UI ---
    // Top controls
    QButtonGroup*  testGroup = nullptr;
    QRadioButton *rbCpu=nullptr, *rbRam=nullptr, *rbGpu=nullptr, *rbDisk=nullptr, *rbNet=nullptr;

    // Options panes
    QWidget *cpuOpts=nullptr,*ramOpts=nullptr,*gpuOpts=nullptr,*diskOpts=nullptr,*netOpts=nullptr;
    QSpinBox *cpuWorkers=nullptr, *cpuDuration=nullptr;
    QSpinBox *ramWorkers=nullptr, *ramDuration=nullptr; QLineEdit* ramBytes=nullptr;
    QLineEdit *diskSize=nullptr, *diskFilename=nullptr; QSpinBox *diskRuntime=nullptr;
    QLineEdit *netServer=nullptr, *netExtra=nullptr;

    // Controls
    QPushButton *btnStart=nullptr,*btnStop=nullptr,*btnClear=nullptr;
    QProgressBar* progress=nullptr; QLabel* eta=nullptr;
    QTextEdit *output=nullptr;

    // Dashboard
    DonutGauge *gCpu=nullptr,*gMem=nullptr,*gDsk=nullptr;

    // Process + timers + logging
    QProcess proc;
    QTimer monitorTimer;
    QElapsedTimer runTimer;
    std::optional<int> expectedSeconds;
    QFile logFile;

    // Theme state
    bool captionColorCoded=false;

    // --- UI setup ---
    void buildUi() {
        // Central widget uses a grid (0=controls, 1=dash, 2=output)
        QWidget *central = new QWidget;
        QGridLayout *grid = new QGridLayout(central);
        grid->setContentsMargins(10,10,10,10);
        grid->setSpacing(8);

        // ===== Menus =====
        QMenu *mFile = menuBar()->addMenu("&File");
        QAction *actSave = mFile->addAction("Save Output As…");
        QAction *actLog  = mFile->addAction("Open Log Folder");
        mFile->addSeparator();
        QAction *actExit = mFile->addAction("Exit");

        QMenu *mTools = menuBar()->addMenu("&Tools");
        QAction *actDeps = mTools->addAction("Check Dependencies");
        QMenu *mTheme = mTools->addMenu("Theme");
        QAction *actLight = mTheme->addAction("Light Mode"); actLight->setCheckable(true); actLight->setChecked(true);
        QAction *actDark  = mTheme->addAction("Dark Mode");  actDark->setCheckable(true);
        QActionGroup *themeGroup = new QActionGroup(this);
        themeGroup->addAction(actLight); themeGroup->addAction(actDark);
        QAction *actColorCaption = mTheme->addAction("Color-code gauge captions"); actColorCaption->setCheckable(true);

        QMenu *mHelp = menuBar()->addMenu("&Help");
        QAction *actAbout = mHelp->addAction("About");

        // store for signals
        connect(actSave,&QAction::triggered,this,&MainWindow::saveOutputAs);
        connect(actLog,&QAction::triggered,this,&MainWindow::openLogFolder);
        connect(actExit,&QAction::triggered,this,&MainWindow::close);
        connect(actDeps,&QAction::triggered,this,&MainWindow::checkDependenciesDialog);
        connect(actLight,&QAction::triggered,this,&MainWindow::applyLightTheme);
        connect(actDark,&QAction::triggered,this,&MainWindow::applyDarkTheme);
        connect(actColorCaption,&QAction::toggled,this,[&](bool on){ captionColorCoded=on; applyCaptionColorMode(); });

        // ===== Row 0: Controls =====
        QWidget* top = new QWidget;
        QVBoxLayout* topv = new QVBoxLayout(top);
        topv->setContentsMargins(0,0,0,0);

        // Test selector
        QWidget* testRow = new QWidget;
        QHBoxLayout* testh = new QHBoxLayout(testRow);
        testh->setContentsMargins(0,0,0,0);
        QLabel* lblSel = new QLabel("Select Test:");
        rbCpu = new QRadioButton("CPU"); rbRam=new QRadioButton("RAM");
        rbGpu = new QRadioButton("GPU"); rbDisk=new QRadioButton("Disk");
        rbNet = new QRadioButton("Network");
        rbCpu->setChecked(true);
        testGroup = new QButtonGroup(this);
        for (auto* r: {rbCpu,rbRam,rbGpu,rbDisk,rbNet}) testGroup->addButton(r);
        testh->addWidget(lblSel); testh->addSpacing(6);
        for (auto* r: {rbCpu,rbRam,rbGpu,rbDisk,rbNet}) { testh->addWidget(r); }
        testh->addStretch(1);
        topv->addWidget(testRow);

        // Options stack
        QStackedWidget* optsStack = new QStackedWidget;
        // CPU
        {
            QWidget* f = new QWidget; QGridLayout* gl = new QGridLayout(f);
            cpuWorkers = new QSpinBox; cpuWorkers->setRange(1,512); cpuWorkers->setValue(std::max(1, QThread::idealThreadCount()));
            cpuDuration= new QSpinBox; cpuDuration->setRange(5, 86400); cpuDuration->setValue(300);
            gl->addWidget(new QLabel("Workers:"),0,0); gl->addWidget(cpuWorkers,0,1);
            gl->addWidget(new QLabel("Duration (s):"),0,2); gl->addWidget(cpuDuration,0,3);
            cpuOpts=f;
        }
        // RAM
        {
            QWidget* f = new QWidget; QGridLayout* gl = new QGridLayout(f);
            ramWorkers = new QSpinBox; ramWorkers->setRange(1,512); ramWorkers->setValue(2);
            ramBytes   = new QLineEdit("1G");
            ramDuration= new QSpinBox; ramDuration->setRange(5,86400); ramDuration->setValue(300);
            gl->addWidget(new QLabel("VM Workers:"),0,0); gl->addWidget(ramWorkers,0,1);
            gl->addWidget(new QLabel("Bytes per VM:"),0,2); gl->addWidget(ramBytes,0,3);
            gl->addWidget(new QLabel("Duration (s):"),0,4); gl->addWidget(ramDuration,0,5);
            ramOpts=f;
        }
        // GPU
        {
            QWidget* f = new QWidget; QHBoxLayout* hl = new QHBoxLayout(f);
            hl->addWidget(new QLabel("glmark2 runs a fixed suite and exits (no duration setting)."));
            gpuOpts=f;
        }
        // Disk
        {
            QWidget* f = new QWidget; QGridLayout* gl = new QGridLayout(f);
            diskSize    = new QLineEdit("1G");
            diskRuntime = new QSpinBox; diskRuntime->setRange(5,3600); diskRuntime->setValue(60);
            diskFilename= new QLineEdit(QDir::currentPath()+"/fio_testfile.bin");
            gl->addWidget(new QLabel("Size:"),0,0); gl->addWidget(diskSize,0,1);
            gl->addWidget(new QLabel("Runtime (s):"),0,2); gl->addWidget(diskRuntime,0,3);
            gl->addWidget(new QLabel("Filename:"),0,4); gl->addWidget(diskFilename,0,5);
            diskOpts=f;
        }
        // Net
        {
            QWidget* f = new QWidget; QGridLayout* gl = new QGridLayout(f);
            netServer = new QLineEdit; netExtra = new QLineEdit;
            gl->addWidget(new QLabel("iperf3 Server IP:"),0,0); gl->addWidget(netServer,0,1);
            gl->addWidget(new QLabel("Extra args (optional):"),0,2); gl->addWidget(netExtra,0,3);
            netOpts=f;
        }
        for (auto* w : {cpuOpts,ramOpts,gpuOpts,diskOpts,netOpts}) optsStack->addWidget(w);
        topv->addWidget(optsStack);

        // Controls row
        QWidget* ctrl = new QWidget; QHBoxLayout* ch = new QHBoxLayout(ctrl);
        btnStart = new QPushButton("Start");
        btnStop  = new QPushButton("Stop"); btnStop->setEnabled(false);
        btnClear = new QPushButton("Clear Output");
        ch->addWidget(btnStart); ch->addWidget(btnStop); ch->addWidget(btnClear); ch->addStretch(1);
        topv->addWidget(ctrl);

        // Progress row
        QWidget* prog = new QWidget; QHBoxLayout* ph = new QHBoxLayout(prog);
        ph->addWidget(new QLabel("Progress:"));
        progress = new QProgressBar; progress->setMinimum(0); progress->setMaximum(100); progress->setValue(0);
        progress->setTextVisible(false);
        ph->addWidget(progress,1);
        eta = new QLabel("ETA: --:--");
        ph->addWidget(eta);
        topv->addWidget(prog);

        grid->addWidget(top,0,0);

        // ===== Row 1: Dashboard =====
        QWidget* dash = new QWidget; QHBoxLayout* dh = new QHBoxLayout(dash);
        dh->setContentsMargins(0,0,0,0);
        dh->addStretch(1);
        gCpu = new DonutGauge; gCpu->setLabel("CPU");    gCpu->setArcColor(QColor("#84cc16"));
        gMem = new DonutGauge; gMem->setLabel("MEMORY"); gMem->setArcColor(QColor("#f59e0b"));
        gDsk = new DonutGauge; gDsk->setLabel("DISK");   gDsk->setArcColor(QColor("#e11d48"));
        dh->addWidget(gCpu); dh->addWidget(gMem); dh->addWidget(gDsk);
        dh->addStretch(1);
        grid->addWidget(dash,1,0);

        // ===== Row 2: Output =====
        output = new QTextEdit; output->setReadOnly(true);
        grid->addWidget(output,2,0);
        grid->setRowStretch(2,1);

        setCentralWidget(central);

        // switching options
        auto pickPane = [optsStack,this](){
            if (rbCpu->isChecked()) optsStack->setCurrentWidget(cpuOpts);
            else if (rbRam->isChecked()) optsStack->setCurrentWidget(ramOpts);
            else if (rbGpu->isChecked()) optsStack->setCurrentWidget(gpuOpts);
            else if (rbDisk->isChecked())optsStack->setCurrentWidget(diskOpts);
            else optsStack->setCurrentWidget(netOpts);
        };
        connect(testGroup, &QButtonGroup::idClicked, this, [pickPane](int){ pickPane(); });
        for (auto* r: {rbCpu,rbRam,rbGpu,rbDisk,rbNet}) connect(r,&QRadioButton::toggled,this,[pickPane](bool){ pickPane(); });
        pickPane();

        statusBar()->showMessage("Ready.");
    }

    void connectSignals() {
        connect(&monitorTimer,&QTimer::timeout,this,&MainWindow::updateDashboard);
        connect(&proc,&QProcess::readyReadStandardOutput,this,&MainWindow::readStdout);
        connect(&proc,&QProcess::readyReadStandardError,this,&MainWindow::readStderr);
        connect(&proc,&QProcess::finished,this,&MainWindow::procFinished);

        connect(btnStart,&QPushButton::clicked,this,&MainWindow::startClicked);
        connect(btnStop,&QPushButton::clicked,this,&MainWindow::stopClicked);
        connect(btnClear,&QPushButton::clicked,output,&QTextEdit::clear);
    }

    // --- Theme ---
    void applyPaletteCommon(const QColor& base, const QColor& text, const QColor& track) {
        QPalette pal = qApp->palette();
        pal.setColor(QPalette::Window, base);
        pal.setColor(QPalette::Base, base.lighter(105));
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Button, base.lighter(102));
        qApp->setPalette(pal);
        setAutoFillBackground(true);

        for (auto* g : {gCpu,gMem,gDsk}) {
            g->setTrackColor(track);
            g->setTextColor(Qt::black);        // per your request: black text
            g->setCaptionColor(Qt::black);
        }
        output->setStyleSheet(QString("QTextEdit{background:%1; color:%2;}").arg(
                                  (base==QColor("#1f2937"))?"#0f172a":"#ffffff",
                                  (base==QColor("#1f2937"))?"#E5E7EB":"#111827"));
    }
    void applyLightTheme() {
        applyPaletteCommon(QColor("#EFEFEF"), QColor("#111827"), QColor("#c7ced6"));
    }
    void applyDarkTheme() {
        applyPaletteCommon(QColor("#1f2937"), QColor("#E5E7EB"), QColor("#0b1620"));
    }
    void applyCaptionColorMode() {
        for (auto* g : {gCpu,gMem,gDsk}) g->setCaptionColorCoded(captionColorCoded);
    }

    // --- Dashboard updates ---
    void updateDashboard() {
        // CPU
        double c = cpuPercent();
        gCpu->setValue(c/100.0, ""); // keep caption blank to avoid clutter

        // Mem
        double u=0,t=0; double mp = memPercent(&u,&t);
        gMem->setValue(mp/100.0, QString("%1 GiB / %2 GiB").arg(QString::number(u,'f',1), QString::number(t,'f',1)));

        // Disk
        double ud=0,td=0; double dp = rootDiskPercent(&ud,&td);
        gDsk->setValue(dp/100.0, QString("%1 GiB / %2 GiB").arg(QString::number(ud,'f',1), QString::number(td,'f',1)));
    }

    // --- Process handling ---
    void startClicked() {
        if (proc.state() != QProcess::NotRunning) {
            QMessageBox::warning(this,"Busy","A test is already running.");
            return;
        }

        auto [cmd, exp] = buildCommand();
        if (cmd.isEmpty()) return;

        // log file
        QString fn = QString("%1/%2_%3.log").arg(logDirPath(), testName(), timestamp());
        logFile.setFileName(fn);
        if (!logFile.open(QIODevice::WriteOnly|QIODevice::Text)) {
            QMessageBox::critical(this, "Logging Error", "Cannot write log file: "+fn);
            return;
        }
        QTextStream ts(&logFile);
        ts << APP_NAME << " Log - " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        ts << "Command: " << cmd.join(' ') << "\n\n"; ts.flush();

        // UI state
        expectedSeconds.reset();
        if (exp.has_value()) expectedSeconds = *exp;
        runTimer.restart();
        btnStart->setEnabled(false);
        btnStop->setEnabled(true);
        progress->setMaximum(exp.has_value()? *exp : 0);
        if (exp.has_value()) { progress->setValue(0); eta->setText("ETA: calculating…"); }
        else { progress->setRange(0,0); eta->setText("ETA: --:--"); } // indeterminate

        // start
        output->append(QString("Starting: %1").arg(cmd.join(' ')));
        statusBar()->showMessage("Running…");
        proc.start(cmd.first(), cmd.mid(1));
        // progress timer via singleShot
        QTimer::singleShot(200, this, &MainWindow::tickProgress);
    }

    void stopClicked() {
        if (proc.state() != QProcess::NotRunning) {
            output->append("\nStopping… attempting graceful termination.");
            proc.terminate();
            if (!proc.waitForFinished(3000)) {
                proc.kill();
            }
            statusBar()->showMessage("Stopping…");
        }
    }

    void procFinished(int rc, QProcess::ExitStatus) {
        output->append(QString("\nProcess finished with return code: %1").arg(rc));
        if (logFile.isOpen()) { QTextStream(&logFile) << "\n[exit] " << rc << "\n"; logFile.close(); }
        btnStart->setEnabled(true);
        btnStop->setEnabled(false);
        statusBar()->showMessage("Ready.");
        progress->setRange(0,100); progress->setValue(0);
        eta->setText("ETA: --:--");
    }

    void readStdout() {
        auto s = QString::fromLocal8Bit(proc.readAllStandardOutput());
        output->moveCursor(QTextCursor::End); output->insertPlainText(s); output->moveCursor(QTextCursor::End);
        if (logFile.isOpen()) { QTextStream(&logFile) << s; }
    }
    void readStderr() {
        auto s = QString::fromLocal8Bit(proc.readAllStandardError());
        output->moveCursor(QTextCursor::End); output->insertPlainText(s); output->moveCursor(QTextCursor::End);
        if (logFile.isOpen()) { QTextStream(&logFile) << s; }
    }

    void tickProgress() {
        if (proc.state() != QProcess::NotRunning) {
            if (expectedSeconds.has_value()) {
                int elapsed = int(runTimer.elapsed()/1000.0);
                progress->setMaximum(*expectedSeconds);
                progress->setValue(std::min(*expectedSeconds, std::max(0, elapsed)));
                int remain = std::max(0, *expectedSeconds - elapsed);
                int mm = remain/60, ss = remain%60;
                eta->setText(QString("ETA: %1:%2").arg(mm,2,10,QChar('0')).arg(ss,2,10,QChar('0')));
            }
            QTimer::singleShot(200, this, &MainWindow::tickProgress);
        }
    }

    // --- Command building / deps ---
    QString testName() const {
        if (rbCpu->isChecked()) return "cpu";
        if (rbRam->isChecked()) return "ram";
        if (rbGpu->isChecked()) return "gpu";
        if (rbDisk->isChecked())return "disk";
        return "net";
    }

    std::pair<QStringList, std::optional<int>> buildCommand() {
        auto need = [&](const QString& exe)->bool{
            if (!which(exe)) {
                QMessageBox::critical(this, "Missing Dependency",
                                      QString("'%1' not found.\nInstall with:\n  sudo apt install %1").arg(exe));
                return false;
            }
            return true;
        };

        if (rbCpu->isChecked()) {
            if (!need("stress-ng")) return {{},std::nullopt};
            int workers = std::max(1, cpuWorkers->value());
            int dur = std::max(5, cpuDuration->value());
            return { {"stress-ng","--cpu",QString::number(workers),"--timeout",QString::number(dur)+"s"}, dur };
        }
        if (rbRam->isChecked()) {
            if (!need("stress-ng")) return {{},std::nullopt};
            int vm = std::max(1, ramWorkers->value());
            int dur= std::max(5, ramDuration->value());
            QString bytes = ramBytes->text().trimmed(); if (bytes.isEmpty()) bytes="512M";
            return { {"stress-ng","--vm",QString::number(vm),"--vm-bytes",bytes,"--timeout",QString::number(dur)+"s"}, dur };
        }
        if (rbGpu->isChecked()) {
            if (!need("glmark2")) return {{},std::nullopt};
            return { {"glmark2"}, std::nullopt };
        }
        if (rbDisk->isChecked()) {
            if (!need("fio")) return {{},std::nullopt};
            QString size = diskSize->text().trimmed(); if (size.isEmpty()) size="1G";
            int runtime = std::max(5, diskRuntime->value());
            QString filename = diskFilename->text().trimmed(); if (filename.isEmpty()) filename = QDir::currentPath()+"/fio_testfile.bin";
            QString ioengine = (QSysInfo::productType()=="linux") ? "libaio" : "psync";
            return { {"fio","--name=randrw","--rw=randrw", "--size="+size,
                      "--runtime="+QString::number(runtime), "--time_based=1",
                      "--filename="+filename, "--ioengine="+ioengine, "--direct=1"}, runtime };
        }
        // net
        {
            if (!need("iperf3")) return {{},std::nullopt};
            QString srv = netServer->text().trimmed();
            if (srv.isEmpty()) {
                QMessageBox::warning(this,"Input Error","Please enter the iperf3 server IP.");
                return {{},std::nullopt};
            }
            QStringList cmd {"iperf3","-c",srv};
            auto extra = QProcess::splitCommand(netExtra->text().trimmed());
            cmd.append(extra);
            return { cmd, std::nullopt };
        }
    }

    void checkDependenciesDialog() {
        QStringList tools = {"stress-ng","glmark2","fio","iperf3"};
        QStringList lines; lines << QString("Dependency check (%1)").arg(QSysInfo::prettyProductName());
        for (const auto& t: tools) {
            QString p; bool ok = which(t,&p);
            lines << QString(" - %1: %2").arg(t, ok?p:"NOT FOUND (sudo apt install "+t+")");
        }
        lines << QString(" - (psutil not needed; using /proc/stat, meminfo, and statvfs)");
        QMessageBox::information(this,"Dependencies",lines.join('\n'));
    }

    // --- File ops ---
    void saveOutputAs() {
        QString fn = QFileDialog::getSaveFileName(this,"Save Output As", QDir::homePath()+"/output.txt",
                                                  "Text files (*.txt);;All files (*)");
        if (fn.isEmpty()) return;
        QFile f(fn);
        if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) {
            QMessageBox::critical(this,"Save Error","Cannot write file.");
            return;
        }
        QTextStream ts(&f); ts << output->toPlainText();
        QMessageBox::information(this,"Save Output", "Saved to:\n"+fn);
    }
    void openLogFolder() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDirPath()));
    }

protected:
    void closeEvent(QCloseEvent* e) override {
        if (proc.state()!=QProcess::NotRunning) {
            auto r = QMessageBox::question(this,"Exit","A test is running. Stop it and exit?");
            if (r != QMessageBox::Yes) { e->ignore(); return; }
            proc.terminate(); if (!proc.waitForFinished(1500)) proc.kill();
        }
        QMainWindow::closeEvent(e);
    }
};

// -----------------------------
// main
// -----------------------------

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    MainWindow w; w.show();
    return app.exec();
}

#include "main.moc"
