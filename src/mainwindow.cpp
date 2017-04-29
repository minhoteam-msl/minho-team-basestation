#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multicastpp.h"

///
/// SIGNAL newRobotInformationReceived is triggered when new information is received from RTDB
///

MainWindow::MainWindow(bool isOfficialField, Multicastpp *coms, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    for(int i=0;i<NROBOTS;i++) robwidgets[i] = NULL;
    ui->setupUi(this);
    robwidgetsReady = false;
    refboxConnected = false;
    refboxSocket = loggerSocket =  NULL;
    ui->statusBar->showMessage("Loading resources ...");
    setupGraphicsUI();
    connectToRefBox();

    preGame = false;
    isCyan = false;
    on_bt_team_clicked();

    mBsInfo.roles.resize(NROBOTS);
    mBsInfo.requests.resize(NROBOTS);
    mBsInfo.gamestate = sSTOPPED;
    mBsInfo.posxside = false;

    iGOALKEEPER = 0;
    iDEFENDERL = 1;
    iDEFENDERR = 2;
    iSUP_STRIKER = 3;
    iSTRIKER = 4;

    auto_role_vec[iGOALKEEPER] = true;
    auto_role_vec[iDEFENDERL] = true;
    auto_role_vec[iDEFENDERR] = true;
    auto_role_vec[iSUP_STRIKER] = true;
    auto_role_vec[iSTRIKER] = true;


    mBsInfo.roles[iGOALKEEPER] = rGOALKEEPER;
    mBsInfo.roles[iSTRIKER] = rSUP_STRIKER;
    mBsInfo.roles[iSUP_STRIKER] = rSTRIKER;

    on_bt_side_clicked();

    rtdb = coms;
    this->isOfficialField = isOfficialField;
    initGazeboBaseStationWorld();
    connect(ui->gzwidget,SIGNAL(newFrameRendered()),this,SLOT(setup3DVisualPtrs()));
    ui->gzwidget->init("mtbasestation");
    ui->gzwidget->setGrid(false);
    ui->gzwidget->setAllControlsMode(false);
    jsonLogger = new cPacketRefboxLogger("MinhoTeam");
    jsonLogger->setTeamIntention("Stop");
    jsonLogger->setAgeMilliseconds(30);
    bsBallVisual = NULL;
    mBsInfo.agent_id = 6;
    run_gz = NULL;

    for(int i=0;i<NROBOTS;i++) {
        robotState[i] = false; robotReceivedPackets[i]=0;
        robotVisuals[i] = ballVisuals[i] = NULL;
        recvFreqs[i] = 0;
        robots[i].agent_id = i+1;
        jsonLogger->updateRobotInformation(robots[i]);
    }

    robotStateDetector = new QTimer();
    sendDataTimer = new QTimer();
    timeOwnGameBall = new QTimer();
    connect(timeOwnGameBall,SIGNAL(timeout()),this,SLOT(setOwnGameBall()));
    connect(robotStateDetector,SIGNAL(timeout()),this,SLOT(detectRobotsState()));
    connect(sendDataTimer,SIGNAL(timeout()),this,SLOT(sendBaseStationUpdate()));
    robotStateDetector->start(300);
    on_comboBox_activated(0);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupGraphicsUI()
{
    QVBoxLayout *layouts = new QVBoxLayout[5];
    ui->frame->setLayout(&layouts[0]);
    layouts[0].addWidget(ui->r1widget);
    layouts[0].setSizeConstraint(QLayout::SetMaximumSize);
    layouts[0].setContentsMargins(0,0,0,0);
    ui->r1widget->setRobotId(1);
    ui->r1widget->setWidgetState(false);
    robwidgets[0] = ui->r1widget;
    //--
    ui->frame_2->setLayout(&layouts[1]);
    layouts[1].addWidget(ui->r2widget);
    layouts[1].setSizeConstraint(QLayout::SetMaximumSize);
    layouts[1].setContentsMargins(0,0,0,0);
    ui->r2widget->setRobotId(2);
    ui->r2widget->setWidgetState(false);
    robwidgets[1] = ui->r2widget;
    //--
    ui->frame_3->setLayout(&layouts[2]);
    layouts[2].addWidget(ui->r3widget);
    layouts[2].setSizeConstraint(QLayout::SetMaximumSize);
    layouts[2].setContentsMargins(0,0,0,0);
    ui->r3widget->setRobotId(3);
    ui->r3widget->setWidgetState(false);
    robwidgets[2] = ui->r3widget;
    //--
    ui->frame_4->setLayout(&layouts[3]);
    layouts[3].addWidget(ui->r4widget);
    layouts[3].setSizeConstraint(QLayout::SetMaximumSize);
    layouts[3].setContentsMargins(0,0,0,0);
    ui->r4widget->setRobotId(4);
    ui->r4widget->setWidgetState(false);
    robwidgets[3] = ui->r4widget;
    //--
    ui->frame_5->setLayout(&layouts[4]);
    layouts[4].addWidget(ui->r5widget);
    layouts[4].setSizeConstraint(QLayout::SetMaximumSize);
    layouts[4].setContentsMargins(0,0,0,0);
    ui->r5widget->setRobotId(5);
    ui->r5widget->setWidgetState(false);
    robwidgets[4] = ui->r5widget;

    robwidgetsReady = true;
    ui->lb_refstate->setText("RefBox ●");
    ui->lb_refstate->setStyleSheet("color:red;");
    ui->lb_refstate->setFont(ui->bt_team->font());// connect reloc and reset signals

    connect(ui->r1widget,SIGNAL(relocRequested(int)),this,SLOT(onRelocRequest(int)));
    connect(ui->r1widget,SIGNAL(resetIMURequested(int)),this,SLOT(onResetIMURequest(int)));
    connect(ui->r2widget,SIGNAL(relocRequested(int)),this,SLOT(onRelocRequest(int)));
    connect(ui->r2widget,SIGNAL(resetIMURequested(int)),this,SLOT(onResetIMURequest(int)));
    connect(ui->r3widget,SIGNAL(relocRequested(int)),this,SLOT(onRelocRequest(int)));
    connect(ui->r3widget,SIGNAL(resetIMURequested(int)),this,SLOT(onResetIMURequest(int)));
    connect(ui->r4widget,SIGNAL(relocRequested(int)),this,SLOT(onRelocRequest(int)));
    connect(ui->r4widget,SIGNAL(resetIMURequested(int)),this,SLOT(onResetIMURequest(int)));
    connect(ui->r5widget,SIGNAL(relocRequested(int)),this,SLOT(onRelocRequest(int)));
    connect(ui->r5widget,SIGNAL(resetIMURequested(int)),this,SLOT(onResetIMURequest(int)));

    connect(ui->r1widget,SIGNAL(auto_roles(int)),this,SLOT(set_auto_roles(int)));
    connect(ui->r2widget,SIGNAL(auto_roles(int)),this,SLOT(set_auto_roles(int)));
    connect(ui->r3widget,SIGNAL(auto_roles(int)),this,SLOT(set_auto_roles(int)));
    connect(ui->r4widget,SIGNAL(auto_roles(int)),this,SLOT(set_auto_roles(int)));
    connect(ui->r5widget,SIGNAL(auto_roles(int)),this,SLOT(set_auto_roles(int)));
}

void MainWindow::updateAgentInfo(void *packet)
{
    // deserialize message
    interAgentInfo agent_data;
    udp_packet *data = (udp_packet*)packet;
    if(!isAgentInfoMessage(data)) return;
    deserializeROSMessage<interAgentInfo>(data,&agent_data);
    delete(data);

    if(agent_data.agent_id<1 || agent_data.agent_id>(NROBOTS));
    else {
      recvFreqs[agent_data.agent_id-1] = 1000.0/(float)data_timers[agent_data.agent_id-1].elapsed();
      data_timers[agent_data.agent_id-1].start();
      robots[agent_data.agent_id-1] = agent_data;
      robotReceivedPackets[agent_data.agent_id-1]++;
      jsonLogger->updateRobotInformation(agent_data);
      emit newRobotInformationReceived(agent_data.agent_id);
    }
    return;
}

bool MainWindow::isAgentInfoMessage(udp_packet *packet)
{
    UInt8 msg;
    ros::serialization::IStream istream(packet->packet, sizeof(msg.data));
    ros::serialization::deserialize(istream, msg);
    if(msg.data==1) return true;
    else return false;
}

void MainWindow::merge_ball_pose()
{
      std::vector <float> bx,by,rx,ry;
      float distancia[NROBOTS];
      float xMain = 0.0,yMain = 0.0,distTotal = 0.0, mediaX = 0.0, mediaY = 0.0;
      int numNow = 0;

      for(unsigned int rob=0;rob<NROBOTS;rob++)
      {
        if(robotState[rob] && robots[rob].agent_info.robot_info.sees_ball)
        {
          rx.push_back(robots[rob].agent_info.robot_info.robot_pose.x);// Adiciona ao vector rx a posição x do robô
          ry.push_back(robots[rob].agent_info.robot_info.robot_pose.y);// Adiciona ao vector ry a posição y do robô
          bx.push_back(robots[rob].agent_info.robot_info.ball_position.x);// Adiciona ao vector bx a posição x da sua bola
          by.push_back(robots[rob].agent_info.robot_info.ball_position.y);// Adiciona ao vector by a posição y da sua bola
          numNow++;
        }
          //.robot_info.robot_pose.x)
      }

      for(unsigned int i=0;i<rx.size();i++)
      {
          float calcAux = (float)sqrt(((bx[i]-rx[i])*(bx[i]-rx[i])+(by[i]-ry[i])*(by[i]-ry[i]))); //Calcula a distância do robô à bola
          distancia[i] = (calcAux);
          mediaX += bx[i]; // Cria media de X
          mediaY += by[i]; // Cria media de Y
      }

      mediaX = mediaX / rx.size();
      mediaY = mediaY / rx.size();

      float distanciasVirtuais[rx.size()], distVirtMedia = 0.0;

      //Calcula a distância virtual em relação às médias
      for(unsigned int i=0;i<rx.size();i++)
      {
          distanciasVirtuais[i] = sqrt(((bx[i]-mediaX)*(bx[i]-mediaX)+(by[i]-mediaY)*(by[i]-mediaY)));
          distVirtMedia+= distanciasVirtuais[i];
      }

      distVirtMedia = distVirtMedia/rx.size();

      int working = 0;

      //Calcula a posição final da bola
      for(unsigned int i=0;i<rx.size();i++)
      {
          if(distanciasVirtuais[i]<=distVirtMedia)
          {
              xMain += bx[i]*(1/distancia[i]);
              yMain += by[i]*(1/distancia[i]);
              distTotal+= (1/distancia[i]);
              working++;
          }
      }

      //Caso alguma das ditâncias tenham sido aceites relativamente à média é apresentada a posição final da bola
      if(working>0)
      {
          xMain = xMain/distTotal;
          yMain = yMain/distTotal;

          mBsInfo.ball.x = xMain;
          mBsInfo.ball.y = yMain;
      }
}

int MainWindow::getRobotByRole(int role)
{
  for(int i =0;i<NROBOTS;i++){
    if(mBsInfo.roles[i]==role) return i;
  }

  return 0;
}

void MainWindow::sendBaseStationUpdate()
{
    static int count = 0;
    // Compute stuff

    // Update Graphics
    updateGraphics();
    ui->statusBar->showMessage("Ξ Rendering at "+QString::number(ui->gzwidget->getAverageFPS())+" fps");
    // Update Roles from robot widgets
    for(unsigned int rob=0;rob<NROBOTS;rob++) {
        if(!auto_role_vec[rob]) mBsInfo.roles[rob] = robwidgets[rob]->getCurrentRole();
        if(robotState[rob] && robwidgets[rob]) robwidgets[rob]->updateInformation(robots[rob].ai_info,robots[rob].hardware_info,recvFreqs[rob]);
    }

    /////////////////////////////////////////////////////////////

    if(robots[iSTRIKER].hardware_info.free_wheel_activated)
    {
        if(!robots[iSUP_STRIKER].hardware_info.free_wheel_activated)
        {
          int tmp = iSUP_STRIKER;
          iSUP_STRIKER = iSTRIKER;
          iSTRIKER = tmp;
        }
    }

    if(mBsInfo.gamestate == sGAME_OWN_BALL)
    {
       //float distance_striker_ball = sqrt((robots[iSTRIKER].agent_info.robot_info.ball_position.x-robots[iSTRIKER].agent_info.robot_info.robot_pose.x)*(robots[iSTRIKER].agent_info.robot_info.ball_position.x-robots[iSTRIKER].agent_info.robot_info.robot_pose.x)+
       //(robots[iSTRIKER].agent_info.robot_info.ball_position.y-robots[iSTRIKER].agent_info.robot_info.robot_pose.y)*(robots[iSTRIKER].agent_info.robot_info.ball_position.y-robots[iSTRIKER].agent_info.robot_info.robot_pose.y));

       float distance_sup_striker_ball = sqrt((robots[iSUP_STRIKER].agent_info.robot_info.ball_position.x-robots[iSUP_STRIKER].agent_info.robot_info.robot_pose.x)*(robots[iSUP_STRIKER].agent_info.robot_info.ball_position.x-robots[iSUP_STRIKER].agent_info.robot_info.robot_pose.x)+
       (robots[iSUP_STRIKER].agent_info.robot_info.ball_position.y-robots[iSUP_STRIKER].agent_info.robot_info.robot_pose.y)*(robots[iSUP_STRIKER].agent_info.robot_info.ball_position.y-robots[iSUP_STRIKER].agent_info.robot_info.robot_pose.y));

        if((!robots[iSTRIKER].agent_info.robot_info.sees_ball && !robots[iSTRIKER].agent_info.robot_info.has_ball))
        {
          if((robots[iSUP_STRIKER].agent_info.robot_info.sees_ball || robots[iSUP_STRIKER].agent_info.robot_info.has_ball) &&  distance_sup_striker_ball < 2)
          {
            int tmp = iSUP_STRIKER;
            iSUP_STRIKER = iSTRIKER;
            iSTRIKER = tmp;
          }
        }
    }

    if(auto_role_vec[iGOALKEEPER]) mBsInfo.roles[iGOALKEEPER] = rGOALKEEPER;

    if(auto_role_vec[iDEFENDERL]) mBsInfo.roles[iDEFENDERL] = rSUP_STRIKER;
    if(auto_role_vec[iDEFENDERR]) mBsInfo.roles[iDEFENDERR] = rSTRIKER;

    if(auto_role_vec[iSUP_STRIKER]) mBsInfo.roles[iSUP_STRIKER] = rSUP_STRIKER;
    if(auto_role_vec[iSTRIKER]) mBsInfo.roles[iSTRIKER] = rSTRIKER;

    for(unsigned int rob=0;rob<NROBOTS;rob++)
    {
      if(auto_role_vec[rob]) robwidgets[rob]->setCurrentRole(mBsInfo.roles[rob]);
    }

    /*
    if(mBsInfo.gamestate == sOWN_KICKOFF && robots[getRobotByRole(rSTRIKER)].agent_info.robot_info.has_ball){
      mBsInfo.gamestate = sGAME_OWN_BALL;
    }
    */

    merge_ball_pose();

    /////////////////////////////////////////////////////////////

    // Send information to Robots
    sendInfoOverMulticast();
    // Generate and send JSON world state
    if(count<5) count++;
    else { sendJSONWorldState(); count = 0; }



}

void MainWindow::sendJSONWorldState()
{
    std::string data = "";
    jsonLogger->getSerialized(data);
    if(refboxSocket) refboxSocket->write(QByteArray::fromStdString(data));
}

void MainWindow::sendInfoOverMulticast()
{
    uint8_t *packet;
    uint32_t packet_size, sentbytes;
    serializeROSMessage<baseStationInfo>(&mBsInfo,&packet,&packet_size);
    // Send packet of size packet_size through UDP
    sentbytes = rtdb->sendData(packet,packet_size);
    if (sentbytes != packet_size){
        ROS_ERROR("Failed to send a packet.");
    }

    for(int i=0;i<NROBOTS;i++) mBsInfo.requests[i] = 0;
}

void MainWindow::initGazeboBaseStationWorld()
{
    std::string gazebouri = "http://127.0.0.1:11346";
    setenv("GAZEBO_MASTER_URI",gazebouri.c_str(),1);
    runGzServer();
}

bool MainWindow::runGzServer()
{
    boost::process::context ctx;
    std::string worldfilename = "";
    if(isOfficialField) worldfilename = "bs_official.world";
    else worldfilename = "bs_lar.world";
    // start gzserver
    std::vector<std::string> args;
    std::string gz = "/usr/bin/gzserver";
    args.push_back(worldfilename);
    // add mathching world file
    //if(run_gz!=NULL){run_gz->terminate(true); delete run_gz; run_gz = NULL;}
    std::map<int,boost::process::handle> a;
    run_gz = new boost::process::child(0,a);
    (*run_gz) = boost::process::create_child(gz, args, ctx);
    usleep(5000000);
}

void MainWindow::setVisibilityRobotGraphics(int robot_id, bool isVisible)
{
    if(robot_id<0||robot_id>NROBOTS-1) return;
    float transp = 1.0;
    if(isVisible) transp = 0.0;
    if(robotVisuals[robot_id]){
        robotVisuals[robot_id]->SetTransparency(transp);
        if(ballVisuals[robot_id]) ballVisuals[robot_id]->SetTransparency(transp);
    } else return;
}

void MainWindow::setVisibilityBsBall(bool isVisible)
{
    float transp = 1.0;
    if(isVisible) transp = 0.0;
    if(bsBallVisual) bsBallVisual->SetTransparency(transp);
}

void MainWindow::updateGraphics()
{
    for(int j=0;j<sceneObstacles.size();j++) {
        scene->RemoveVisual(sceneObstacles[j]);
        sceneObstacles[j].reset();
    }

    sceneObstacles.clear();
    int onRobots = 0;
    for(int i=0;i<NROBOTS;i++){
        robwidgets[i]->setWidgetState(robotState[i]);
        if(robotState[i]){
            // show stuff from robot i
            setVisibilityRobotGraphics(i,true);
                // update robot position
                setRobotPose(i+1,robots[i].agent_info.robot_info.robot_pose.x,
                             robots[i].agent_info.robot_info.robot_pose.y,
                             robots[i].agent_info.robot_info.robot_pose.z);
                // if sees ball, update ball position, else, hide ball
                if(robots[i].agent_info.robot_info.sees_ball){
                    float ballHeight = 0.11;
                    if(robots[i].is_goalkeeper) ballHeight = robots[i].agent_info.robot_info.ball_position.z;
                        ballHeight = robots[i].agent_info.robot_info.ball_position.z;
                    setBallPosition(i+1,robots[i].agent_info.robot_info.ball_position.x,
                                        robots[i].agent_info.robot_info.ball_position.y,
                                        ballHeight);
                } else if(ballVisuals[i]) ballVisuals[i]->SetTransparency(1.0);

            onRobots++;

            // Draw obstacles
            for(int k=0;k<robots[i].agent_info.robot_info.obstacles.size();k++){
                if(robots[i].agent_info.robot_info.obstacles[k].isenemy){
                    VisualPtr newObs;
                    std::string visualname = "obstacle"+std::to_string(sceneObstacles.size());
                    newObs.reset(new Visual(visualname.c_str(), scene));
                    newObs->Load();
                    newObs->AttachMesh("unit_sphere");
                    newObs->SetScale(math::Vector3(0.5, 0.5, 0.8));
                    newObs->SetCastShadows(false);
                    newObs->SetMaterial("Gazebo/Black");
                    newObs->SetVisibilityFlags(GZ_VISIBILITY_GUI);
                    newObs->SetWorldPosition(math::Vector3(robots[i].agent_info.robot_info.obstacles[k].x,
                                                           -robots[i].agent_info.robot_info.obstacles[k].y,0.4));
                    sceneObstacles.push_back(newObs);
                }
            }
        } else {
            // hide stuff from robot i
            setVisibilityRobotGraphics(i,false);
            //mBsInfo.roles[i] = rSTOP;
            data_timers[i].restart();
            //jsonLogger->removeRobot(robots[i].agent_id);
        }
    }

    if(onRobots>0){
        setVisibilityBsBall(true);
        bsBallVisual->SetWorldPosition(math::Vector3(mBsInfo.ball.x,-mBsInfo.ball.y,0.11));

    } else setVisibilityBsBall(false);
}

void MainWindow::setCameraPose(float x, float y, float z, float yaw, float pitch, float roll)
{
    ui->gzwidget->setCameraPose(Vector3d(x,y,z),Vector3d(yaw,pitch,roll));
}

void MainWindow::setRobotPose(int robot_id, float x, float y, float z)
{
    if(robot_id<1||robot_id>NROBOTS) return;
    std::string robotprefix = "robot_";
    std::string modelName = robotprefix+std::to_string(robot_id);
    ui->gzwidget->setModelPoseInWorld(modelName,Vector3d(x,y,z));
}

void MainWindow::setBallPosition(int ball_id, float x, float y, float z)
{
    if(ball_id<0||ball_id>NROBOTS) return;
    std::string ballprefix = "ball_";
    std::string bsballname = "ball_bs";
    std::string modelName = bsballname;
    if(ball_id>0) modelName = ballprefix+std::to_string(ball_id);
    ui->gzwidget->setModelPoseInWorld(modelName,Vector3d(x,y,z));
}

bool MainWindow::connectToRefBox()
{
    if(refboxSocket) {
        refboxSocket->close();
        refboxSocket->abort();
        delete refboxSocket;
    }

    refboxSocket = new QTcpSocket();

    QString refboxIP = "127.0.0.1";
    // red iptable.cfg from common
    std::string ipFilePath = getenv("HOME");
    std::string line = "";
    ipFilePath += "/Common/iptable.cfg";
    std::ifstream file; file.open(ipFilePath);
    if(file.is_open()){
        while (getline(file,line)){
            if(line.size()>0 && line[0]!='#'){
                if(line.find("RB")!=std::string::npos){
                    refboxIP = QString::fromStdString(line.substr(0,line.find(" ")));
                    ROS_INFO("Found Refbox IP config at iptable.cfg - %s",refboxIP.toStdString().c_str());
                }
            }
        }
        file.close();
    } else ROS_ERROR("Failed to read iptable.cfg");
    ui->lb_refstate->setText(QString("RefBox [")+refboxIP+QString("] ●"));
    ui->lb_refstate->setFont(ui->bt_team->font());
    if(refboxIP=="127.0.0.1") ROS_INFO("Setting RefBox IP as localhost.");

    int enableKeepAlive = 1;
    int fd = refboxSocket->socketDescriptor();
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enableKeepAlive, sizeof(enableKeepAlive));
    int maxIdle = 5; /* seconds */
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &maxIdle, sizeof(maxIdle));
    int count = 3;  // send up to 3 keepalive packets out, then disconnect if no response
    setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count));
    int interval = 2;   // send a keepalive packet out every 2 seconds (after the 5 second idle period)
    setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));

    refboxSocket->connectToHost(QHostAddress(refboxIP),28097);
    connect(refboxSocket,SIGNAL(readyRead()),this,SLOT(onRefBoxData()));
    connect(refboxSocket,SIGNAL(disconnected()),this,SLOT(onRefBoxDisconnection()));
}

void MainWindow::detectRobotsState()
{
    for(int i=0;i<NROBOTS;i++) {
        if(robotReceivedPackets[i]>4) robotState[i] = true;
        else robotState[i] = false;
        robotReceivedPackets[i] = 0;
    }
}

void MainWindow::setup3DVisualPtrs()
{
    scene = ui->gzwidget->getScene();
    std::string robotprefix = "robot_";
    std::string ballprefix = "ball_";
    std::string bsballname = "ball_bs";

    bsBallVisual = scene->GetVisual(bsballname);
    if(bsBallVisual!=NULL){
        bsBallVisual = bsBallVisual->GetRootVisual();
        disconnect(ui->gzwidget,SIGNAL(newFrameRendered()),this,SLOT(setup3DVisualPtrs()));
        sendDataTimer->start(CYCLE_TIME);
    }else return;

    for(int i=0;i<NROBOTS;i++){
        robotVisuals[i] = scene->GetVisual(robotprefix+std::to_string(i+1))->GetRootVisual();
        ballVisuals[i] = scene->GetVisual(ballprefix+std::to_string(i+1))->GetRootVisual();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    ui->gzwidget->close();
    system("pkill -f \"gzserver bs_\" ");
    int status = 0;
    if(run_gz)waitpid(run_gz->get_id(), &status, WNOHANG);
    event->accept();
}

template<typename Message>
void MainWindow::deserializeROSMessage(udp_packet *packet, Message *msg)
{
    ros::serialization::IStream istream(packet->packet, packet->packet_size);
    ros::serialization::deserialize(istream, *msg);
}

template<typename Message>
void MainWindow::serializeROSMessage(Message *msg, uint8_t **packet, uint32_t *packet_size)
{
    uint32_t serial_size = ros::serialization::serializationLength( *msg );
    serialization_buffer.reset(new uint8_t[serial_size]);
    (*packet_size) = serial_size;
    ros::serialization::OStream stream( serialization_buffer.get(), serial_size );
    ros::serialization::serialize( stream, *msg);
    (*packet) = serialization_buffer.get();
}

void MainWindow::on_bt_team_clicked()
{
    isCyan = !isCyan;
    QPalette pal = ui->bt_team->palette();
    if(isCyan){ pal.setColor(QPalette::Button,QColor(Qt::cyan)); ui->bt_team->setText("Team CYAN"); }
    else { pal.setColor(QPalette::Button,QColor(Qt::magenta)); ui->bt_team->setText("Team MAGENTA"); }
    ui->bt_team->setPalette(pal);
}

void MainWindow::on_bt_side_clicked()
{
    mBsInfo.posxside = !mBsInfo.posxside;
    QPalette pal = ui->bt_side->palette();
    if(mBsInfo.posxside){ pal.setColor(QPalette::Button,QColor(Qt::yellow)); ui->bt_side->setText("Right Side"); }
    else { pal.setColor(QPalette::Button,QColor(Qt::red)); ui->bt_side->setText("Left Side"); }
    ui->bt_side->setPalette(pal);
}

void MainWindow::on_bt_conref_clicked()
{
    connectToRefBox();
}

void MainWindow::onRefBoxData()
{
    QString command = refboxSocket->readAll();
    ROS_INFO("Received %s",command.toStdString().c_str());
    if(command == "S")
    { // stop,end part or end half
        mBsInfo.gamestate = sSTOPPED;
    }
    else if(command == "W")
    { // on connection with refbox
        refboxConnected = true;
        ui->lb_refstate->setStyleSheet("color:green;");
        ui->lb_refstate->setFont(ui->bt_team->font());
    }
    else if(command == "e" || command == "h")
    { // end part or end half
        mBsInfo.gamestate = sSTOPPED;
        timeOwnGameBall->stop();
        if(command=="h") on_bt_side_clicked();
    }
    else if(command == "L")
    { // parking
        mBsInfo.gamestate = sPARKING;
        if(command=="h") on_bt_side_clicked();
    }
    else if(command == "s" || command == "1s" || command == "2s")
    { //start
        switch (mBsInfo.gamestate) {
          case sPRE_OWN_KICKOFF:
          mBsInfo.gamestate = sOWN_KICKOFF;
          break;
          case sPRE_THEIR_KICKOFF:
          mBsInfo.gamestate = sTHEIR_KICKOFF;
          break;
          case sPRE_OWN_GOALKICK:
          mBsInfo.gamestate = sOWN_GOALKICK;
          break;
          case sPRE_THEIR_GOALKICK:
          mBsInfo.gamestate = sTHEIR_GOALKICK;
          break;
          case sPRE_OWN_FREEKICK:
          mBsInfo.gamestate = sOWN_FREEKICK;
          break;
          case sPRE_THEIR_FREEKICK:
          mBsInfo.gamestate = sTHEIR_FREEKICK;
          break;
          case sPRE_OWN_PENALTY:
          mBsInfo.gamestate = sOWN_PENALTY;
          break;
          case sPRE_THEIR_PENALTY:
          mBsInfo.gamestate = sTHEIR_PENALTY;
          break;
          case sPRE_OWN_THROWIN:
          mBsInfo.gamestate = sOWN_THROWIN;
          break;
          case sPRE_THEIR_THROWIN:
          mBsInfo.gamestate = sTHEIR_THROWIN;
          break;
          case sPRE_OWN_CORNER:
          mBsInfo.gamestate = sOWN_CORNER;
          break;
          case sPRE_THEIR_CORNER:
          mBsInfo.gamestate = sTHEIR_CORNER;
          break;
          default:
          mBsInfo.gamestate = sSTOPPED;
          break;
        }
        timeOwnGameBall->start(7000);
    }
    else if(command == "K" || command == "k")
    { // kickoff
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_KICKOFF;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_KICKOFF;
        else mBsInfo.gamestate = sPRE_THEIR_KICKOFF;
    } else if(command == "F" || command == "f"){ // freekick
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_FREEKICK;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_FREEKICK;
        else mBsInfo.gamestate = sPRE_THEIR_FREEKICK;
    } else if(command == "G" || command == "g"){ // goalkick
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_GOALKICK;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_GOALKICK;
        else mBsInfo.gamestate = sPRE_THEIR_GOALKICK;
    }else if(command == "P" || command == "p"){ // penalty
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_PENALTY;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_PENALTY;
        else mBsInfo.gamestate = sPRE_THEIR_PENALTY;
    }else if(command == "T" || command == "t"){ // throw in
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_THROWIN;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_THROWIN;
        else mBsInfo.gamestate = sPRE_THEIR_THROWIN;
    }else if(command == "C" || command == "c"){ // corner
        if(isCyan && command[0].isUpper()) mBsInfo.gamestate = sPRE_OWN_CORNER;
        else if(!isCyan && command[0].isLower()) mBsInfo.gamestate = sPRE_OWN_CORNER;
        else mBsInfo.gamestate = sPRE_THEIR_CORNER;
    }else mBsInfo.gamestate = sSTOPPED;

    qDebug() <<  mBsInfo.gamestate;
}

void MainWindow::onRefBoxDisconnection()
{
    mBsInfo.gamestate = sSTOPPED;
    ui->lb_refstate->setStyleSheet("color:red;");
    ui->lb_refstate->setFont(ui->bt_team->font());
}

void MainWindow::on_comboBox_activated(int index)
{
    switch(index){
        case 0:{
            if(isOfficialField) setCameraPose(0.0,-22,12,0,0.55,1.58);
            break;
        }
        case 1:{
            if(isOfficialField) setCameraPose(18.5,-19,10,0,0.41,2.27);
            break;
        }
        case 2:{
            if(isOfficialField) setCameraPose(-17,-19.5,10,0,0.41,0.9);
            break;
        }
        case 3:{
            if(isOfficialField) setCameraPose(0.0,0.0,33,0,1.57,1.57);
            break;
        }
    }
}

void MainWindow::onRelocRequest(int id)
{
    mBsInfo.requests[id-1] = 1;
}

void MainWindow::onResetIMURequest(int id)
{
    mBsInfo.requests[id-1] = 2;
}

void MainWindow::set_auto_roles(int id)
{
    auto_role_vec[id-1] = !auto_role_vec[id-1];
    //qDebug() << "Auto roles: " << id-1 << " state: " << auto_role_vec[id-1];
}

void MainWindow::setOwnGameBall()
{
    if(mBsInfo.gamestate!=sSTOPPED) mBsInfo.gamestate = sGAME_OWN_BALL;
    timeOwnGameBall->stop();
}
