#include "mainWindow.h"

/*#############################################################################
#
#   Layout methods
#
#############################################################################*/


MainWindow::MainWindow() {
// Constructor for the main interface

    centralWidget = new QWidget(this);
    leftBox       = new QWidget(this);
    // Allow the leftBox to expand vertically, but not horizontally
    leftBox->setSizePolicy(QSizePolicy(QSizePolicy::Fixed,QSizePolicy::Expanding));
    rightBox      = new QSplitter(Qt::Vertical, this);
    createPlotPanel();

    network = new Network("mynetwork",false);
    simulator = NULL;
    networkPlot = new GraphWidget();
    netAnalysisDialog = new QDialog(this);
    netAnalysisDialog->setWindowTitle("Analysis of current network");
    rep_ct = 0;

    
    logEditor = new QTextEdit();
    logEditor->setReadOnly(true);
    logEditor->setPlainText(tr("No output yet"));
   
    QHBoxLayout *mainLayout = new QHBoxLayout;
    QVBoxLayout *leftLayout = new QVBoxLayout;

    createControlButtonsBox();
    createNetworkSettingsBox();
    createSimulatorSettingsBox();
    defaultSettings();
    
    leftLayout->addWidget(networkSettingsGroupBox, Qt::AlignCenter);    
    leftLayout->addWidget(simulatorSettingsGroupBox, Qt::AlignCenter);    
    leftLayout->addWidget(logEditor);
    leftLayout->addWidget(controlButtonsGroupBox);
    
    leftBox->setLayout(leftLayout);
    leftBox->setContentsMargins(0,0,0,0);

    setWindowTitle(tr("EpiFire"));

    createMenu();
    

    mainLayout->setMenuBar(menuBar);
    mainLayout->addWidget(leftBox);
    mainLayout->addWidget(rightBox);
    centralWidget->setLayout(mainLayout);

    setCentralWidget(centralWidget);
    statusBar()->showMessage(generateNetMsg);

    createNetworkAnalysis();

    progressDialog = new QProgressDialog("", "Cancel", 0, 100);
    progressDialog->setWindowTitle("EpiFire status");
    //progressDialog = new QProgressDialog("", "Cancel", 0, 100, this);
    progressDialog->setWindowModality(Qt::WindowModal);

    backgroundThread = new BackgroundThread(this);
    connect(backgroundThread,SIGNAL(completed(bool)),this,SLOT(netDoneUpdate(bool)));
    connect(backgroundThread,SIGNAL(completed(bool)),this,SLOT(updateNetworkPlot()));
    connect(backgroundThread,SIGNAL(finished()), this, SLOT(resetCursor()));
    connect(this, SIGNAL(progressUpdated(int)),progressDialog,SLOT(setValue(int)));
    connect(progressDialog,SIGNAL(canceled()),this,SLOT(stopBackgroundThread()));
    connect(progressDialog,SIGNAL(accepted()),this,SLOT(disableCentralWidget()));
    connect(progressDialog,SIGNAL(rejected()),this,SLOT(enableCentralWidget()));
}


void MainWindow::createPlotPanel() {
    epiCurvePlot = new PlotArea(this, "Epidemic curves");
    epiCurvePlot->setPlotType(PlotArea::CURVEPLOT);
    epiCurvePlot->setToolTip("Absolute frequency of infectious nodes vs. time\nDouble-click plot to save image\nRight-click to save data");

    statePlot = new PlotArea(this, "Node state evolution");
    statePlot->setPlotType(PlotArea::STATEPLOT);
    statePlot->setToolTip("Progression of node states over time for 100 nodes\nBlue = susceptible, Red = infectious, Yellow = Recovered\nDouble-click plot to save image\nRight-click to save data");

    histPlot = new PlotArea(this, "Histogram of epidemic sizes");
    histPlot->setPlotType(PlotArea::HISTPLOT);
    histPlot->setToolTip("Distribution of final epidemic sizes\nDouble-click plot to save image\nRight-click to save data");
 
    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(statePlot);
    rightLayout->addWidget(epiCurvePlot);
    rightLayout->addWidget(histPlot);
    
    rightBox->setLayout(rightLayout);     
}


void MainWindow::createMenu() {
    //Create 'File' menu
    menuBar = new QMenuBar;

    fileMenu = new QMenu(tr("&File"), this);

    exitAction = fileMenu->addAction(tr("E&xit"));
    openAction = fileMenu->addAction(tr("&Open"));

    QAction* simulateAction = fileMenu->addAction("Simulate");
    simulateAction->setShortcut(Qt::Key_Enter);

    QAction* saveNetwork = fileMenu->addAction("Save network as edgelist");
    QAction* saveDataAction  = fileMenu->addAction("Save epidemic curve data");
    QAction* savePlotAction = fileMenu->addAction("Save epidemic curve plot");

    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));
    connect(openAction, SIGNAL(triggered()), this, SLOT(readEdgeList()));
    connect(simulateAction, SIGNAL(triggered()), this, SLOT(simulatorWrapper()));
    connect(saveNetwork, SIGNAL(triggered()), this, SLOT(saveEdgeList()));
    connect(saveDataAction, SIGNAL(triggered()), epiCurvePlot, SLOT(saveData()));
    connect(savePlotAction, SIGNAL(triggered()), epiCurvePlot, SLOT(savePlot()));
 
    //Create 'Plot' menu
    QMenu* plotMenu = new QMenu(tr("&Plot"), this);
    QAction* showNetworkPlot = plotMenu->addAction("Show network plot");
    plotMenu->addSeparator();
    
    showStatePlot = plotMenu->addAction("Show node state plot");
    showStatePlot->setCheckable(true);
    showStatePlot->setChecked(true);
    
    showEpiPlot = plotMenu->addAction("Show epidemic curve plot");
    showEpiPlot->setCheckable(true);
    showEpiPlot->setChecked(true);
    
    showHistPlot = plotMenu->addAction("Show histogram");
    showHistPlot->setCheckable(true);
    showHistPlot->setChecked(true);

    connect(showNetworkPlot, SIGNAL(triggered()), this, SLOT(plotNetwork()));
    connect(showStatePlot, SIGNAL(triggered()), this, SLOT( showHideStatePlot() ));
    connect(showEpiPlot, SIGNAL(triggered()), this, SLOT( showHideEpiCurvePlot() ));
    connect(showHistPlot, SIGNAL(triggered()), this, SLOT( showHideHistPlot() ));
    connect(rightBox, SIGNAL(splitterMoved(int, int)), this, SLOT( updatePlotMenuFlags() ));

    //Create 'Network' menu
    QMenu* networkMenu = new QMenu(tr("&Network"), this);
    QAction* showNetworkAnalysis = networkMenu->addAction("Network analysis");
    QAction* reduceToGiantComponent = networkMenu->addAction("Remove all minor components");

    connect( showNetworkAnalysis, SIGNAL(triggered()), this, SLOT(analyzeNetwork()));
    connect( reduceToGiantComponent, SIGNAL(triggered()), this, SLOT(removeMinorComponents()));

    menuBar->addMenu(fileMenu);
    menuBar->addMenu(plotMenu);
    menuBar->addMenu(networkMenu);
}

void MainWindow::updateProgress(int x) {
     emit progressUpdated(x);
}

void MainWindow::createNetworkSettingsBox() {
// Creates the main input forms and their labels

    // Define text boxes
    numnodesLine = new QLineEdit();
    numnodesLine->setAlignment(Qt::AlignRight);
    numnodesLine->setValidator( new QIntValidator(2,INT_MAX,numnodesLine) );
        
    poiLambdaLine = new QLineEdit();
    poiLambdaLine->setAlignment(Qt::AlignRight);
    poiLambdaLine->setValidator( new QDoubleValidator(0.0, numeric_limits<double>::max(), 20, poiLambdaLine) );
    expBetaLine = new QLineEdit();
    expBetaLine->setAlignment(Qt::AlignRight);
    expBetaLine->setValidator( new QDoubleValidator(expBetaLine) );
    powAlphaLine = new QLineEdit();
    powAlphaLine->setAlignment(Qt::AlignRight);
    powAlphaLine->setValidator( new QDoubleValidator(powAlphaLine) );
    conValueLine = new QLineEdit();
    conValueLine->setAlignment(Qt::AlignRight);
    conValueLine->setValidator( new QIntValidator(0.0,INT_MAX,conValueLine) );
    powKappaLine = new QLineEdit();
    powKappaLine->setAlignment(Qt::AlignRight);
    powKappaLine->setValidator( new QDoubleValidator(0.0, numeric_limits<double>::max(), 20, powKappaLine) );
    
    netsourceLabel = new QLabel(tr("Network source:"));
    netfileLabel = new QLabel(tr("Filename"));
    netfileLine = new QLineEdit();

    netsourceBox= new QComboBox(this);
    netsourceBox->addItem("Generate");
    netsourceBox->addItem("Load from file");

    // Define all of the labels, in order of appearance

    QLabel *numnodesLabel = new QLabel(tr("Number of nodes:"));
    distLabel = new QLabel(tr("Degree distribution:"));
    param1Label = new QLabel(tr(""));
    param2Label = new QLabel(tr(""));

    // Build degree distribution dropdown box
    distBox = new QComboBox;
    distBox->addItem("Poisson");
    distBox->addItem("Exponential");
    distBox->addItem("Power law");
    distBox->addItem("Urban");
    distBox->addItem("Constant");

    // Initialize layout to parameters for first distribution listed, and listen for changes
    defaultNetworkParameters();
    connect(distBox,SIGNAL(currentIndexChanged (int)), this, SLOT(changeNetworkParameters(int)));

    changeNetSource(0);
    connect(netsourceBox,SIGNAL(currentIndexChanged (int)), this, SLOT(changeNetSource(int)));

    // Put everything together
    networkSettingsGroupBox = new QGroupBox(tr("Step 1: Choose a network"));
    networkSettingsGroupBox->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed));
    QGridLayout *layout = new QGridLayout;

    //Network source
    layout->addWidget(netsourceLabel, 0, 0);
    layout->addWidget(netsourceBox, 0, 1);
    
    //fields for imported net
    layout->addWidget(netfileLabel, 2, 0);
    layout->addWidget(netfileLine, 2, 1);

    //fields for generated net
    layout->addWidget(numnodesLabel, 1, 0);
    layout->addWidget(numnodesLine, 1, 1);
    layout->addWidget(distLabel, 2, 0);
    layout->addWidget(distBox, 2, 1);
    layout->addWidget(param1Label, 3, 0);
    layout->addWidget(poiLambdaLine, 3, 1);
    layout->addWidget(expBetaLine, 3, 1);
    layout->addWidget(powAlphaLine, 3, 1);
    layout->addWidget(conValueLine, 3, 1);
    layout->addWidget(param2Label, 4, 0);
    layout->addWidget(powKappaLine, 4, 1);
    
    networkSettingsGroupBox->setLayout(layout);
}

void MainWindow::createSimulatorSettingsBox() {
    simLabel = new QLabel("Simulation type");
    simBox  =  new QComboBox(this);
    simBox->addItem("Chain Binomial");
    simBox->addItem("Percolation");

    QLabel *numrunsLabel = new QLabel(tr("Number of runs:"));
    numrunsLine = new QLineEdit();
    numrunsLine->setAlignment(Qt::AlignRight);
    numrunsLine->setValidator( new QIntValidator(1,10000,numrunsLine) );
    rzeroLine = new QLineEdit();
    makeReadonly(rzeroLine);
    rzeroLine->setAlignment(Qt::AlignRight);
    rzeroLine->setToolTip("Expected number of secondary infections caused\nby each infection early in the epidemic");
    
    transLine = new QLineEdit();
    transLine->setAlignment(Qt::AlignRight);
    transLine->setValidator( new QDoubleValidator(0.0,1.0,20,transLine) );
    transLine->setToolTip("Probability of transmission from infectious to susceptible neighbor\nRange: 0 to 1");
    
    pzeroLine = new QLineEdit();
    pzeroLine->setAlignment(Qt::AlignRight);
    pzeroLine->setValidator( new QIntValidator(1,INT_MAX,numrunsLine) );
    pzeroLine->setToolTip("Number of randomly chosen individuals to start epidemic\nRange: positive integers");

    infectiousPeriodLine = new QLineEdit();
    infectiousPeriodLine->setAlignment(Qt::AlignRight);
    infectiousPeriodLine->setValidator( new QIntValidator(1,INT_MAX,infectiousPeriodLine) );
    infectiousPeriodLine->setToolTip("Duration of infectious state (units = time steps)\nRange: positive integers");

    QLabel *pzeroLabel = new QLabel(tr("Patient zero count:"));
    QLabel *rzeroLabel = new QLabel(tr("Expected R-zero:"));
    QLabel *transLabel = new QLabel(tr("Transmissibility:"));
    infectiousPeriodLabel = new QLabel(tr("Infectious period:"));
    changeSimType(0); 
    connect(simBox,SIGNAL(currentIndexChanged (int)), this, SLOT(changeSimType(int)));
    connect(transLine,            SIGNAL(textChanged(QString)), this, SLOT(updateRZero()));
    connect(infectiousPeriodLine, SIGNAL(textChanged(QString)), this, SLOT(updateRZero()));

    //Build checkbox
    retainDataCheckBox = new QCheckBox(tr("Retain data between runs"));

    // Put everything together
    simulatorSettingsGroupBox = new QGroupBox(tr("Step 2: Design a simulation"));
    simulatorSettingsGroupBox->setSizePolicy(QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed));
    QGridLayout *layout = new QGridLayout;

    //SECOND COLUMN -- Simulation stuff
    layout->addWidget(simLabel, 0, 1);
    layout->addWidget(simBox, 0, 2);
    layout->addWidget(infectiousPeriodLabel, 1, 1);
    layout->addWidget(infectiousPeriodLine, 1, 2);
    layout->addWidget(transLabel, 2, 1);
    layout->addWidget(transLine, 2, 2);
    layout->addWidget(rzeroLabel, 3, 1);
    layout->addWidget(rzeroLine, 3, 2);
    layout->addWidget(pzeroLabel, 4, 1);
    layout->addWidget(pzeroLine, 4, 2);
    layout->addWidget(numrunsLabel, 5, 1);
    layout->addWidget(numrunsLine, 5, 2);
    layout->addWidget(retainDataCheckBox,6,1,2,2);

    simulatorSettingsGroupBox->setLayout(layout);
}


void MainWindow::createControlButtonsBox() {
//Creates the horizontal control box at the bottom of the interface

    controlButtonsGroupBox = new QGroupBox(tr("Step 3: Profit!"));
    QGridLayout *layout    = new QGridLayout;
    
    clearNetButton = new QPushButton("Clear Network");
    connect(clearNetButton, SIGNAL(clicked()), this, SLOT(clear_network()));
    layout->addWidget(clearNetButton, 0, 0);
    clearNetButton->setEnabled(false);
    
    defaultSettingsButton = new QPushButton("Default Settings");
    connect(defaultSettingsButton, SIGNAL(clicked()), this, SLOT(defaultSettings()));
    layout->addWidget(defaultSettingsButton, 0, 1);

    loadNetButton     = new QPushButton("Import Edge List");
    connect(loadNetButton,     SIGNAL(clicked()), this, SLOT(readEdgeList()));
    layout->addWidget(loadNetButton, 0, 2);

    generateNetButton = new QPushButton("Generate Network");
    connect(generateNetButton, SIGNAL(clicked()), this, SLOT(generate_network_thread()));
    //connect(generateNetButton, SIGNAL(clicked()), this, SLOT(updateNetworkPlot()));
    layout->addWidget(generateNetButton, 0, 2);

    clearDataButton = new QPushButton("Clear data");
    connect(clearDataButton, SIGNAL(clicked()), this, SLOT(clear_data()));
    layout->addWidget(clearDataButton, 1, 0);
    clearDataButton->setEnabled(false);

    //helpButton = new QPushButton("Help");
    //connect(helpButton, SIGNAL(clicked()), this, SLOT(open_help()));
    //layout->addWidget(helpButton, 1, 1);

    analyzeNetButton = new QPushButton("Analyze network");
    connect(analyzeNetButton, SIGNAL(clicked()), this, SLOT(analyzeNetwork()) );
    layout->addWidget(analyzeNetButton, 1, 1);


    runSimulationButton = new QPushButton("Run &Simulation");
    connect(runSimulationButton, SIGNAL(clicked()), this, SLOT(simulatorWrapper()));
    runSimulationButton->setDefault(true);
    layout->addWidget(runSimulationButton, 1, 2);
    runSimulationButton->setEnabled(false);

    controlButtonsGroupBox->setLayout(layout);
}


/*#############################################################################
#
#   IO methods
#
#############################################################################*/


void MainWindow::saveEdgeList() {

    if(!network || network->size() == 0) { appendOutputLine("No network to save.");   return;}

    QString startdir = ".";
    QString filename = QFileDialog::getSaveFileName(
        this, "Select file to save to:", startdir, "All Files(*.*)");

    if (filename.size() == 0) return;

    network->write_edgelist(filename.toStdString());
}


void MainWindow::readEdgeList() {
    QString startdir = ".";
    QStringList filelist = QFileDialog::getOpenFileNames(
        this, "Select edge list file to load:", startdir, "All Files(*.*)");

    if (filelist.size() == 0) { 
        appendOutputLine("Error: edge list file is empty");
        return;
    }
    QString fileName = filelist[0];

    if(network) { delete(network); }
    netComponents.clear();

    setCursor(Qt::WaitCursor);
    appendOutputLine("Importing network . . . ");
    network = new Network("mynetwork", false);
    network->read_edgelist(fileName.toStdString());
    //network->dumper();
    netfileLine->setText(fileName);
    numnodesLine->setText(QString::number(network->size()));
    netDoneUpdate(true);
    //updateRZero();
    //setCursor(Qt::ArrowCursor);
}


void MainWindow::appendOutput(QString s) {
// Used to append output to the main textbox
    logEditor->moveCursor( QTextCursor::End) ;
    logEditor->insertPlainText(s);
}


// Used to append new 'paragraph' to the main textbox
void MainWindow::appendOutputLine(QString s) { 
    logEditor->append(s); 
    logEditor->moveCursor( QTextCursor::End) ;
}


void MainWindow::makeEditable(QLineEdit* lineEdit) {
    lineEdit->setReadOnly(false);
    QPalette pal = lineEdit->palette();
    pal.setColor(lineEdit->backgroundRole(), Qt::white);
    lineEdit->setPalette(pal);
}


void MainWindow::makeReadonly(QLineEdit* lineEdit) {
    lineEdit->setReadOnly(true);
    QPalette pal = lineEdit->palette();
    pal.setColor(lineEdit->backgroundRole(), Qt::transparent);
    lineEdit->setPalette(pal);
}


/*#############################################################################
#
#   Update methods
#
#############################################################################*/


void MainWindow::defaultSettings() {
//Resets GUI to its default settings (as specified in .h file)
    netsourceBox->setCurrentIndex(0);
    distBox->setCurrentIndex(0);
    defaultNetworkParameters();
    numnodesLine->setText(default_network_size);

    simBox->setCurrentIndex(0);
    if (simBox->currentIndex() == 1) changeSimType(0);
    infectiousPeriodLine->setText(default_infectious_pd);
    transLine->setText(default_T);
    numrunsLine->setText(default_num_runs);
    pzeroLine->setText(default_P0);
    retainDataCheckBox->setChecked(true);
}


void MainWindow::changeNetSource(int source) {
    if(source == 1 ) {           // load net from file
        netfileLabel->show();
        netfileLine->show();
        makeReadonly(netfileLine);
        makeReadonly(numnodesLine);
        loadNetButton->show();
        generateNetButton->hide();
        if (netfileLine->text() == "") { // it would be better if we had a flag to check
            numnodesLine->setText("0");  // whether the network had been read from a file
            statusBar()->showMessage(loadNetMsg);
        } else {
            numnodesLine->setText(QString::number(network->size()));
            statusBar()->showMessage(simulateMsg);
        }
        distBox->hide();
        distLabel->hide();
        changeNetworkParameters(3);
    }                            // generate random net
    else {
        netfileLabel->hide();
        netfileLine->hide();
        loadNetButton->hide();
        generateNetButton->show();

        distBox->show();
        distLabel->show();
        numnodesLine->setText(default_network_size);
        makeEditable(numnodesLine);

        changeNetworkParameters(distBox->currentIndex());
        if (netfileLine->text() == "" and network->size() > 0) { // it would be better if we had a flag to check
            statusBar()->showMessage(simulateMsg);
        } else {
            statusBar()->showMessage(generateNetMsg);
        }
    }
}
// parameter line edits should not be recycled.  it's the only way to remember what users were doing before.

void MainWindow::defaultNetworkParameters() {
//Changes the labels for the parameter boxes, and grays them out as appropriate
        expBetaLine->hide();
        powAlphaLine->hide();
        conValueLine->hide();
        expBetaLine->setText(default_exp_param1);
        powAlphaLine->setText(default_pow_param1);
        conValueLine->setText(default_con_param1);

        param2Label->setText("Kappa:");
        param2Label->hide();
        powKappaLine->hide();
        powKappaLine->setText(default_pow_param2);
        
        param1Label->setText("Lambda:");
        param1Label->show();
        poiLambdaLine->setText(default_poi_param1);
        poiLambdaLine->show();
}

void MainWindow::changeNetworkParameters(int dist_type) {
//Changes the labels for the parameter boxes, and shows/hides them out as appropriate

    if (dist_type == 0) { // Poisson
        expBetaLine->hide();
        powAlphaLine->hide();
        conValueLine->hide();
        param2Label->hide();
        powKappaLine->hide();
        
        param1Label->setText("Lambda:");
        param1Label->show();
        poiLambdaLine->show();
    }
    else if (dist_type == 1) { // Exponential
        poiLambdaLine->hide();
        powAlphaLine->hide();
        conValueLine->hide();
        param2Label->hide();
        powKappaLine->hide();

        param1Label->setText("Beta:");
        param1Label->show();
        expBetaLine->show();
     }
    else if (dist_type == 2) { // Power law
        poiLambdaLine->hide();
        expBetaLine->hide();
        conValueLine->hide();

        param1Label->setText("Alpha:");
        param1Label->show();
        powAlphaLine->show();
        param2Label->setText("Kappa:");
        param2Label->show();
        powKappaLine->show();
    }
    else if (dist_type == 3) { // Urban
        param1Label->hide();
        poiLambdaLine->hide();
        expBetaLine->hide();
        powAlphaLine->hide();
        conValueLine->hide();
        param2Label->hide();
        powKappaLine->hide();
    }
    else if (dist_type == 4) { // Constant
        poiLambdaLine->hide();
        expBetaLine->hide();
        powAlphaLine->hide();
        param2Label->hide();
        powKappaLine->hide();

        param1Label->setText("Fixed degree:");
        param1Label->show();
        conValueLine->show();
    }
}


void MainWindow::changeSimType(int type) {
    epiCurvePlot->clearData();
    epiCurvePlot->replot();
    if (type == 0) { // Chain Binomial
        double T = (transLine->text()).toDouble();
        int d = (infectiousPeriodLine->text()).toInt();
        transLine->setText( QString::number( convertTtoTCB(T, d) ) );
        infectiousPeriodLabel->show();
        infectiousPeriodLine->show();
    
    } else { // Percolation
        double TCB = (transLine->text()).toDouble();
        int d = (infectiousPeriodLine->text()).toInt();
        transLine->setText( QString::number( convertTCBtoT(TCB, d) ) );
        infectiousPeriodLabel->hide();
        infectiousPeriodLine->hide();
    }
}


void MainWindow::clear_data() {
    epiCurvePlot->clearData();
    epiCurvePlot->replot();

    statePlot->clearData();
    statePlot->replot();
    
    histPlot->clearData();
    histPlot->replot();

    rep_ct = 0;
    appendOutputLine("Epidemic data deleted");
    clearDataButton->setEnabled(false);
    statusBar()->showMessage(clearedDataMsg, 1000);
}


void MainWindow::clear_network() {
    if(network) network->clear_nodes();
    netComponents.clear();
    updateRZero();
    appendOutputLine("Network deleted");
    runSimulationButton->setEnabled(false);
    netfileLine->setText("");
    clearNetButton->setEnabled(false);
    statusBar()->showMessage(clearedNetMsg, 1000);
}

bool MainWindow::validateParameters() { 
    double T = (transLine->text()).toDouble();
    if (T < 0 || T > 1.0) {
        appendOutputLine("Transmissibility must be between 0.0 and 1.0"); 
        return false;
    }
   return true; 
}


void MainWindow::updateRZero() {
    if (!network || network->size() == 0 || !validateParameters()) {
        rzeroLine->setText( "Undefined" );
        return;
    }

    double T = (transLine->text()).toDouble();
    int d = (infectiousPeriodLine->text()).toInt();
    if ( simBox->currentText() == "Chain Binomial") {
        T = convertTCBtoT(T, d); // convert to perc's transmissibility
    }

    double R0 = convertTtoR0(T); 
    rzeroLine->setText( QString::number(R0));
}


/*#############################################################################
#
#   Epidemiology/network methods
#
#############################################################################*/


void MainWindow::simulatorWrapper() {
//Connects the GUI information to the percolation simulator
    if (!network || network->size() == 0 ) { appendOutputLine("Network must be generated first."); return; }

    // Get values from textboxes
    //int j_max = (numrunsLine->text()).toInt();
    double T = (transLine->text()).toDouble();
    //int p_zero = (pzeroLine->text()).toInt();
    //string RunID="1";            // This needs to be updated
    //int dist_size_array[j_max];  //Initiate array that will contain distribution sizes
    //int* dist_size_point=dist_size_array;

    //CREATE SIMULATOR
    if(simulator) { delete(simulator); simulator=NULL; }

    if ( simBox->currentText() == "Chain Binomial") {
        int infectious_pd = (infectiousPeriodLine->text()).toInt();
        simulator = new ChainBinomial_Sim(network, infectious_pd, T);
    }
    else {
        simulator = new Percolation_Sim(network);
        ((Percolation_Sim*) simulator)->set_transmissibility(T);
    }

    bool retain_data = retainDataCheckBox->isChecked();
    
    if (! retain_data) {
        epiCurvePlot->clearData();
        epiCurvePlot->replot(); // a bit clumsy, but it works
    }

    //RUN SIMULATION
    generate_sim_thread();

    // it would be nice to replace disabling buttons with an event handler
    runSimulationButton->setEnabled(false);
    generateNetButton->setEnabled(false);
    loadNetButton->setEnabled(false);

    while (backgroundThread->isRunning()) {
        qApp->processEvents();
        //  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    runSimulationButton->setEnabled(true);
    generateNetButton->setEnabled(true);
    loadNetButton->setEnabled(true);

    setCursor(Qt::ArrowCursor);
    clearDataButton->setEnabled(true);
    statusBar()->showMessage(simDoneMsg, 1000);
    progressDialog->setLabelText("");

    //MAKE PLOTS
    epiCurvePlot->replot();
    statePlot->replot();
    histPlot->replot();

}


void MainWindow::addStateData() {
    vector<int> node_states(100);
    for (int i = 0; i < network->size() && i < node_states.size(); i++) {
        node_states[i] = (int) network->get_node(i)->get_state();
    }

    statePlot->addData(node_states);
}

void MainWindow::updatePlotMenuFlags() {
    QList<int> sizes = rightBox->sizes();

    if (sizes[0] == 0) { showStatePlot->setChecked(false); } 
    else { showStatePlot->setChecked(true); }

    if (sizes[1] == 0) { showEpiPlot->setChecked(false); }
    else { showEpiPlot->setChecked(true); }

    if (sizes[2] == 0) { showHistPlot->setChecked(false); }
    else { showHistPlot->setChecked(true); }
}

void MainWindow::showHideStatePlot() {
    QList<int> old = rightBox->sizes();
    QList<int> newSizes;
    int H = old[0] + old[1] + old[2];
    if (H == old[0]) { // means we're trying to close the only pane that's open
        showStatePlot->setChecked(true);
        return;
    }
    
    if (old[0] == 0) { 
        newSizes << H/3 << 2*old[1]/3 << 2*old[2]/3;
        showStatePlot->setChecked(true);
    } else {
        newSizes << 0 << H * old[1]/(H-old[0]) << H * old[2]/(H-old[0]);
        showStatePlot->setChecked(false);
    }

    rightBox->setSizes(newSizes);
}

void MainWindow::showHideEpiCurvePlot() {
    QList<int> old = rightBox->sizes();
    QList<int> newSizes;
    int H = old[0] + old[1] + old[2];
    if (H == old[1]) { // means we're trying to close the only pane that's open
        showEpiPlot->setChecked(true);
        return;
    }
        
    if (old[1] == 0) {
        newSizes << 2*old[0]/3 << H/3 << 2*old[2]/3;
        showEpiPlot->setChecked(true);
    } else {
        newSizes << H * old[0]/(H-old[1]) << 0 << H * old[2]/(H-old[1]);
        showEpiPlot->setChecked(false);
    }

    rightBox->setSizes(newSizes);
}

void MainWindow::showHideHistPlot() {
    QList<int> old = rightBox->sizes();
    QList<int> newSizes;
    int H = old[0] + old[1] + old[2];
    if (H == old[2]) { // means we're trying to close the only pane that's open
        showHistPlot->setChecked(true);
        return;
    }
    
    if (old[2] == 0) {
        newSizes << 2*old[0]/3 << 2*old[1]/3 << H/3 ;
        showHistPlot->setChecked(true);
    } else {
        newSizes << H * old[0]/(H-old[2]) << H * old[1]/(H-old[2]) << 0;
        showHistPlot->setChecked(false);
    }

    rightBox->setSizes(newSizes);
}

void MainWindow::updateNetworkPlot() {
    if(networkPlot->isVisible() == false) return;
    plotNetwork();
}


void MainWindow::plotNetwork() { 
    if (!network || network->size() == 0) {
        appendOutputLine("Please generate network first");
        return;
    } else if ( network->size() > 500 ) {
        appendOutputLine("Network is too large to draw (500 node limit; < 100 nodes is recommended)");
        return;
    }

    networkPlot->clear();
    vector<Edge*> edges = network->get_edges();
    map<Edge*, bool> seen;
    for( int i=0; i < edges.size(); i++ ) {
        if (seen.count(edges[i]->get_complement())) continue;
        seen[edges[i]] = true;
        int id1 = edges[i]->get_start()->get_id();
        int id2 = edges[i]->get_end()->get_id();
        string name1 = QString::number(id1).toStdString();
        string name2 = QString::number(id2).toStdString();
        GNode* n1 = networkPlot->addGNode(name1,0);
        GNode* n2 = networkPlot->addGNode(name2,0);
        GEdge* e = networkPlot->addGEdge(n1,n2,"edgeTag",0);
    }
    networkPlot->setLayoutAlgorithm(GraphWidget::Circular);
    networkPlot->newLayout();
    networkPlot->show();
}

void MainWindow::_addAnalysisRow(QGridLayout* layout, QString text, QLineEdit* box, QPushButton* button) {
    int r = layout->rowCount();
    QLabel* label = new QLabel(text, netAnalysisDialog);
    layout->addWidget(label, r, 0);
    layout->addWidget(box, r, 1);
    if (button) layout->addWidget(button, r, 2);
}

void MainWindow::createNetworkAnalysis() {
    QVBoxLayout* netAnalysisLayout = new QVBoxLayout();
    //QHBoxLayout* netTopLayout = new QHBoxLayout();
    QGridLayout* netTopLayout = new QGridLayout();
    //QVBoxLayout* netTopRightLayout = new QVBoxLayout();

    nodeCountEdit        = new QLineEdit(netAnalysisDialog);
    edgeCountEdit        = new QLineEdit(netAnalysisDialog);
    meanDegreeEdit       = new QLineEdit(netAnalysisDialog);
    componentCountEdit   = new QLineEdit(netAnalysisDialog);
    maxComponentSizeEdit = new QLineEdit(netAnalysisDialog);
    transitivityEdit     = new QLineEdit(netAnalysisDialog);
    diameterEdit         = new QLineEdit(netAnalysisDialog);
    meanDistanceEdit     = new QLineEdit(netAnalysisDialog);

    makeReadonly(nodeCountEdit);
    makeReadonly(edgeCountEdit);
    makeReadonly(meanDegreeEdit);
    makeReadonly(componentCountEdit);
    makeReadonly(maxComponentSizeEdit);
    makeReadonly(transitivityEdit);
    makeReadonly(diameterEdit);
    makeReadonly(meanDistanceEdit);

    componentButton1   = new QPushButton("Calculate", netAnalysisDialog);
    componentButton2   = new QPushButton("Calculate", netAnalysisDialog);
    transitivityButton = new QPushButton("Calculate", netAnalysisDialog);
    diameterButton     = new QPushButton("Calculate", netAnalysisDialog);
    meanDistanceButton = new QPushButton("Calculate", netAnalysisDialog);

    connect(componentButton1,   SIGNAL(clicked()), this, SLOT(generate_comp_thread()));
    connect(componentButton2,   SIGNAL(clicked()), this, SLOT(generate_comp_thread()));
    connect(transitivityButton, SIGNAL(clicked()), this, SLOT(generate_trans_thread()));
    connect(diameterButton,     SIGNAL(clicked()), this, SLOT(generate_dist_thread()));
    connect(meanDistanceButton, SIGNAL(clicked()), this, SLOT(generate_dist_thread()));

    _addAnalysisRow(netTopLayout, "Node count:",         nodeCountEdit);
    _addAnalysisRow(netTopLayout, "Edge count:",         edgeCountEdit);
    _addAnalysisRow(netTopLayout, "Mean degree:",        meanDegreeEdit);
    _addAnalysisRow(netTopLayout, "Largest component:",  maxComponentSizeEdit, componentButton1);
    _addAnalysisRow(netTopLayout, "Component count:",    componentCountEdit, componentButton2 );
    _addAnalysisRow(netTopLayout, "Transitivity:",       transitivityEdit, transitivityButton);
    _addAnalysisRow(netTopLayout, "Diameter:",           diameterEdit, diameterButton);
    _addAnalysisRow(netTopLayout, "Mean shortest path:", meanDistanceEdit, meanDistanceButton);

    QGroupBox* netAnalysisTop = new QGroupBox();
    netAnalysisTop->setLayout(netTopLayout);

    degDistPlot = new PlotArea(this, "Degree distribution");
    degDistPlot->setPlotType(PlotArea::DEGPLOT);
    
    // add a close window button
    QPushButton* closeButton = new QPushButton("Close analysis", netAnalysisDialog);
    connect(closeButton,  SIGNAL(clicked()), netAnalysisDialog, SLOT(close()));
    QHBoxLayout* buttonBoxLayout   = new QHBoxLayout();
    QWidget* buttonBox = new QWidget();
    buttonBoxLayout->addStretch(1);
    buttonBoxLayout->addWidget(closeButton);
    buttonBox->setLayout(buttonBoxLayout);

    netAnalysisLayout->addWidget(netAnalysisTop);
    netAnalysisLayout->addWidget(degDistPlot);
    netAnalysisLayout->addWidget(buttonBox);
    
    netAnalysisDialog->setLayout(netAnalysisLayout);
}


void MainWindow::calculateComponentStats() {
    if ( netComponents.empty() ) netComponents = network->get_components();
    int count = netComponents.size();
    int biggest = 0;

    for (unsigned int i = 0; i<netComponents.size(); i++) {
        if (netComponents[i].size() > (unsigned) biggest) {
            biggest = netComponents[i].size();
        }
    }

    componentCountEdit   ->setText(QString::number( count ));
    maxComponentSizeEdit ->setText(QString::number( biggest ));
    //setCursor(Qt::ArrowCursor);
}


void MainWindow::removeMinorComponents() {
    generate_comp_thread();
    // this block should instead call a wait function
    // or something similarly concise
    runSimulationButton->setEnabled(false);
    generateNetButton->setEnabled(false);
    loadNetButton->setEnabled(false);

    while (backgroundThread->isRunning()) {
        qApp->processEvents();
    }
    runSimulationButton->setEnabled(true);
    generateNetButton->setEnabled(true);
    loadNetButton->setEnabled(true);
    // end block


    vector<Node*> giant;
    unsigned int i;
    cerr << "num componenets: " << netComponents.size() << endl;
    for (i = 0; i < netComponents.size(); i++) {
cerr << "i, size: " << i << ", " << netComponents[i].size() << endl;
        if (netComponents[i].size() > network->size()/2) {
            giant = netComponents[i];
cerr << "giant (i): " << i << endl;
            break;
        }
    }
    if (giant.empty()) {
        appendOutputLine("Network was not reduced: no giant component");
    } else {
        for (unsigned int j = 0; j < netComponents.size(); j++) {
            if (j == i) continue;
cerr << "deleting " << j << ", size " << netComponents[j].size() << endl;
            foreach (Node* n, netComponents[j]) {
                network->delete_node(n);
            }
        }
        netComponents.clear();
        netComponents.push_back(giant);

        numnodesLine->setText(QString::number(network->size()));
        updateRZero();
    }


}

void MainWindow::analyzeNetwork() {
    //progressDialog->setParent(netAnalysisDialog);
    if (!network or network->size() == 0) {
        QMessageBox msgBox;
        msgBox.setText("Please generate or import a network first.");
        msgBox.exec();
        return;
    }
    nodeCountEdit        ->setText(QString::number( network->size() ));
    int edge_ct = network->get_edges().size();
    edge_ct = network->is_directed() ? edge_ct : edge_ct / 2;
    edgeCountEdit        ->setText(QString::number( edge_ct ));
    meanDegreeEdit       ->setText(QString::number( network->mean_deg() ));
    componentCountEdit   ->clear();
    maxComponentSizeEdit ->clear();
    transitivityEdit     ->clear();
    diameterEdit         ->clear();
    meanDistanceEdit     ->clear();

    degDistPlot->clearData();
    degDistPlot->addData(network->get_deg_series());
    degDistPlot->replot();

    netAnalysisDialog->exec();
}


void MainWindow::calculateTransitivity() {
    if (!network) return;
    vector<Node*> empty;
    transitivityEdit->setText(QString::number( network->transitivity(empty) ));
}


void MainWindow::calculateDistances() {
    if (!network) return;
    progressDialog->setLabelText("Finding biggest component ...");
    calculateComponentStats();
    vector<Node*>* giant_comp = &netComponents[0];

    // locate the biggest component in the network
    for (unsigned int i = 1; i<netComponents.size(); i++) {
        if (netComponents[i].size() > (*giant_comp).size()) {
            giant_comp = &netComponents[i];
        }
    }
    
    progressDialog->setLabelText("Finding shortest paths in component ...");
    // calculate the shortest path lengths within it
    //for(unsigned int t = 0; t< (*giant_comp).size(); t++) cerr << (*giant_comp)[t]->get_id() << endl;
    vector< vector<int> > pathLengths = network->calculate_unweighted_distances(*giant_comp);
    double diam = 0.0;
    double mean = 0.0;
    for (unsigned int i = 0; i<pathLengths.size(); i++) {
        double node_mean = 0.0;
        for (unsigned int j = 0; j<pathLengths[i].size(); j++) {
            if (i==j) continue;
            //cerr << pathLengths[i][j] << "\t";
            diam = pathLengths[i][j] > diam ? pathLengths[i][j] : diam;
            node_mean += pathLengths[i][j];
        }
        //cerr << endl;
        node_mean /= pathLengths[i].size() - 1;
        mean += node_mean;
    }
    
    mean /= pathLengths.size();
    diameterEdit->setText(QString::number( diam ));
    meanDistanceEdit->setText(QString::number( mean ));
}

void MainWindow::resetCursor() { setCursor(Qt::ArrowCursor); }

void MainWindow::enableCentralWidget() { centralWidget->setEnabled(true); }

void MainWindow::disableCentralWidget() { centralWidget->setEnabled(false); }

void MainWindow::generate_network_thread() {
    statusBar()->showMessage(busyNetMsg);
    setCursor(Qt::WaitCursor);
    appendOutputLine("Generating network . . . ");

    if(network) delete(network);
    netComponents.clear();
    netfileLine->setText("");

    int n = (numnodesLine->text()).toInt();
    network = new Network("mynetwork", false);
    network->populate(n);

    backgroundThread->setThreadType(BackgroundThread::GENERATE_NET);

    progressDialog->setLabelText("Generating network");
    backgroundThread->start();
}


void MainWindow::generate_sim_thread() {
    setCursor(Qt::WaitCursor);
    backgroundThread->setThreadType(BackgroundThread::SIMULATE);
    progressDialog->setLabelText("Simulation running");
    backgroundThread->start();
}


void MainWindow::generate_comp_thread() {
    setCursor(Qt::WaitCursor);
    backgroundThread->setThreadType(BackgroundThread::COMPONENTS);
    progressDialog->setLabelText("Determining network components");
    backgroundThread->start();
}

void MainWindow::generate_trans_thread() {
    setCursor(Qt::WaitCursor);
    backgroundThread->setThreadType(BackgroundThread::TRANSITIVITY);
    progressDialog->setLabelText("Calculating transitivity clustering coefficient");
    backgroundThread->start();
}


void MainWindow::generate_dist_thread() {
    setCursor(Qt::WaitCursor);
    backgroundThread->setThreadType(BackgroundThread::DISTANCES);
    progressDialog->setLabelText("Beginning shortest path calculation ...");
    backgroundThread->start();
}


void MainWindow::stopBackgroundThread() {
     if (backgroundThread) backgroundThread->stop();
     //cerr << "thread supposedly stopped\n";
     appendOutputLine("Process interrupted.");
}

bool MainWindow::generate_network() {
    DistType dist_type = (DistType) distBox->currentIndex();
    double par1 = 0.0;
    double par2 = 0.0;

    if (dist_type == POI) {
        par1 = (poiLambdaLine->text()).toDouble();
    } else if (dist_type == EXP) {
        par1 = (expBetaLine->text()).toDouble();
    } else if (dist_type == POW) {
        par1 = (powAlphaLine->text()).toDouble();
        par2 = (powKappaLine->text()).toDouble();
    } else if (dist_type == CON) {
        par1 = (conValueLine->text()).toDouble();
    } 

    // 'true' on success, 'false' if interrupted or impossible
    return connect_network(network, dist_type, par1, par2);

}


void MainWindow::netDoneUpdate(bool success) {
    if ( success ) {
        updateRZero();
        appendOutput("Done.");
        setCursor(Qt::ArrowCursor);
        runSimulationButton->setEnabled(true);
        clearNetButton->setEnabled(true);
        statusBar()->showMessage(simulateMsg);
    } else {
        setCursor(Qt::ArrowCursor);
        clear_network();
        statusBar()->showMessage(generateNetMsg);
    }
}


bool MainWindow::connect_network (Network* net, DistType dist, double param1, double param2) {
    if (dist == POI) {
        if (param1 <= 0) { appendOutputLine("Poisson distribution parameter must be > 0"); return false; }
        return net->fast_random_graph(param1);
    }
    else if (dist == EXP) {
        if (param1 <= 0) { appendOutputLine("Exponential distribution parameter must be > 0"); return false; }
        return net->rand_connect_exponential(param1);
    }
    else if (dist == POW) {
        if (param2 <= 0) { appendOutputLine("Exponential distribution kappa parameter must be > 0"); return false; }
        return net->rand_connect_powerlaw(param1, param2);
    }
    else if (dist == URB) {
        vector<double> dist;
        double deg_array[] = {0, 0, 1, 12, 45, 50, 73, 106, 93, 74, 68, 78, 91, 102, 127, 137, 170, 165, 181, 181, 150, 166, 154, 101, 67, 69, 58, 44, 26, 24, 17, 6, 11, 4, 0, 6, 5, 3, 1, 1, 3, 1, 1, 0, 1, 0, 2};
        dist.assign(deg_array,deg_array+47);
        dist = normalize_dist(dist, sum(dist));
        return net->rand_connect_user(dist);
    }
    else if (dist == CON) {
        if (( (int) param1 * net->size()) % 2 == 1) {
            appendOutputLine("The sum of all degrees must be even\nThis is not possible with the network parameters you have specified");
            return false;
        }
        vector<double> dist(param1+1, 0);
        dist[param1] = 1;
        return net->rand_connect_user(dist);
    }
}


double MainWindow::calculate_T_crit() {
    vector<double> dist = network->get_gen_deg_dist();
    double numerator = 0;// mean degree, (= <k>)
    double denominator = 0;
    for (unsigned int k=1; k < dist.size(); k++) {
        numerator += k * dist[k];
        denominator += k * (k-1) * dist[k];
    }
    return  numerator/denominator;
}

/*
double MainWindow::guessEpiSize(double R0, double P0) {
    //This calculation is based on the expected epidemic size
    //for a mass action model. See Tildesley & Keeling (JTB, 2009).
    double S0 = 1.0 - P0;
    for (double p = 0.01; p <= 1.0; p += 0.01) {
        cerr << "i: " << p << endl;
        if (S0*(1-exp(-R0 * p)) <= p) return p;
    }
    return 1.0;
}*/


double MainWindow::guessEpiSize(double R0, double P0, double guess) {
    //This calculation is based on the expected epidemic size
    //for a mass action model. See Tildesley & Keeling (JTB, 2009).
    //cerr << "r: " << guess << endl;
    double S0 = 1.0 - P0;
    double p = S0*(1-exp(-R0 * guess));
    if (fabs(p-guess) < 0.0001) {return p;}
    else return guessEpiSize(R0, P0, p);
}

/*
double MainWindow::guessEpiSizeB(double R0, double P0) {
    //This calculation is based on the expected epidemic size
    //for a mass action model. See Tildesley & Keeling (JTB, 2009).
    cerr << "b" << endl;
    double guess = 0.5;
    double S0 = 1.0 - P0;
    for (int i = 0; i < 10; i++) {
        cerr << "b: " << guess << endl;
        double p = S0*(1-exp(-R0 * guess));
        if (guess < p) {
            guess += 1.0/pow(2, i+2);
        } else if (guess > p) {
            guess -= 1.0/pow(2, i+2);
        }
    }
    return guess;
}
*/

double MainWindow::convertR0toT(double R0) { return R0 * calculate_T_crit(); }


double MainWindow::convertTtoR0(double T) { return T / calculate_T_crit(); }


double MainWindow::convertTtoTCB (double T, int d) { return 1.0 - pow(1.0 - T, 1.0/(double) d); }


double MainWindow::convertTCBtoT (double TCB, int d) { return 1.0 - pow(1.0 - TCB, d); }


int MainWindow::percent_complete(int current, double predicted) { return current > predicted ? 99 : (int) (100 * current/predicted); }
