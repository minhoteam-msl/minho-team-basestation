

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include "multicastpp.h"
#include <QCloseEvent>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QHostAddress>
#include <QLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <ros/ros.h>
#include <Utils/types.h>
#include <minho_team_ros/interAgentInfo.h> // msg_id = 1;
#include <minho_team_ros/robotInfo.h>
#include <minho_team_ros/hardwareInfo.h>
#include <minho_team_ros/baseStationInfo.h> // msg_id = 0;
#include <std_msgs/UInt8.h>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <gzwidget.h>
#include <robotwidget.h>
#include <FalconsJSON/cPacketRefboxLogger.hpp>

using minho_team_ros::interAgentInfo;
using minho_team_ros::robotInfo;
using minho_team_ros::hardwareInfo;
using minho_team_ros::baseStationInfo;
using std_msgs::UInt8;

#define CYCLE_TIME 33 //in ms
#define NROBOTS 5

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(bool isOfficialField, Multicastpp *coms, QWidget *parent = 0);
    ~MainWindow();
    /// \brief function that parses upd packet into interAgentInfo message or baseStationInfo message.
    void updateAgentInfo(void *packet);
    /// \brief close event override
    void closeEvent(QCloseEvent *event);
    /// \brief function to setup graphical stuff in ui
    void setupGraphicsUI();

private:
    /// \brief function that reads msg_id parameter in packet and returns true if msg_id = 1 (is interAgentInfo)
    bool isAgentInfoMessage(udp_packet *packet);
    /// \brief templated function to deserialize udp_packet into ROS message
    template<typename Message>
    void deserializeROSMessage(udp_packet *packet, Message *msg);
    /// \brief function that serializes baseStationInfo into UDP packet
    template<typename Message>
    void serializeROSMessage(Message *msg, uint8_t **packet, uint32_t *packet_size);
    /// \brief function that sends information over multicast socket
    void sendInfoOverMulticast();
    /// \brief creates a system process to hold gazebo for base station
    void initGazeboBaseStationWorld();
    /// \brief init gazebo for base station
    bool runGzServer();
    /// \brief hides/shows every component belonging to robot
    void setVisibilityRobotGraphics(int robot_id, bool isVisible);
    /// \brief hides/shows bs ball
    void setVisibilityBsBall(bool isVisible);
    /// \brief function where updates to graphics should be done
    void updateGraphics();
    /// \brief sets camera pose (x,y,z)(yaw,pitch,roll)
    void setCameraPose(float x,float y,float z,float yaw, float pitch, float roll);
    /// \brief sets robot position (x,y) and orientation (z) in degrees (1,2,3,4,5) for the robots
    void setRobotPose(int robot_id, float x,float y,float z);
    /// \brief sets ball position (x,y,z) (0 for bs ball, 1,2,3,4,5 for robot's ball)
    void setBallPosition(int ball_id, float x, float y,float z);
    /// \brief function to connect via TCP socket to refbox
    bool connectToRefBox();

    int getRobotByRole(int role);


private slots:
    /// \brief function that detects if a robot is online or offline based on received packets
    void detectRobotsState();
    /// \brief function that retrieves visual pointers to all non-static elements in 3D world
    void setup3DVisualPtrs();
    /// \brief slot function to compute and send current baseStationInfo message
    void sendBaseStationUpdate();
    /// \brief function to generate and send JSON log for refbox visualizer
    void sendJSONWorldState();
    /// \brief print test function
    void printSlot(QString info) { ROS_INFO("Slot: %s",info.toStdString().c_str()); }
    /// \brief slot for changing team color
    void on_bt_team_clicked();
    /// \brief slot for changing playing side
    void on_bt_side_clicked();
    /// \brief button to reconnect to refbox
    void on_bt_conref_clicked();
    /// \brief slot for received data from refbox
    void onRefBoxData();
    /// \brief slot on disconnection of refbox socket
    void onRefBoxDisconnection();
    /// \brief slot to change visualization camera
    void on_comboBox_activated(int index);
    /// \brief slot to set data for requesting a reloc
    void onRelocRequest(int id);
    ///  \brief slot to set data for requesting a IMU reset
    void onResetIMURequest(int id);
    ///  \function to merge ball pose
    void merge_ball_pose();
    /// auto roles ON/OFF
    void set_auto_roles(int id);

    void setOwnGameBall();
signals:
    void newRobotInformationReceived(int agent_id);
private:
    Ui::MainWindow *ui;
    /// \brief vector that holds most recent info from a robot
    interAgentInfo robots[NROBOTS];
    /// \brief vector that holds if a robot is online or offline
    bool robotState[NROBOTS];
    /// \brief vector to hold number of received packets by a certain agent
    int robotReceivedPackets[NROBOTS];
    /// \brief vector of pointers to robot widgets
    robotWidget *robwidgets[NROBOTS];
    /// \brief timer that calls a function to detect if an agent is online or offline
    QTimer *robotStateDetector, *sendDataTimer;
    /// \brief pointer to multicast class, to send info
    Multicastpp *rtdb;
    /// \brief buffer to use in message serialization
    boost::shared_array<uint8_t> serialization_buffer;
    /// \brief object that holds info to be sent to robots
    baseStationInfo mBsInfo;
    /// \brief variable that tells if used field is oficial or lar
    bool isOfficialField;
    /// \brief process handler for gazebo
    boost::process::child *run_gz;
    /// \brief pointer to 3D scene
    ScenePtr scene;
    /// \brief vector to hold visual pointers to all robots
    VisualPtr robotVisuals[NROBOTS];
    /// \brief vector to hold visual pointers to all balls
    VisualPtr ballVisuals[NROBOTS];
    /// \brief vector to hold visual pointer to basestation ball
    VisualPtr bsBallVisual;
    /// \brief tells if widgets have been set up
    bool robwidgetsReady;
    /// \brief variables to compute receive frequency of robot data
    QTime data_timers[NROBOTS]; float recvFreqs[NROBOTS];
    /// \brief holds team color
    bool isCyan;
    /// \brief socket to interface with refbox
    QTcpSocket *refboxSocket, *loggerSocket;
    /// \brief holds if refbox is connected or not
    bool refboxConnected;
    /// \brief refbox worldstate logger object
    cPacketRefboxLogger *jsonLogger;
    /// \brief pointer to base obstacle
    VisualPtr obstacleVisual;
    /// \brief vector of obstacles drawn on scene
    std::vector<VisualPtr> sceneObstacles;
    /// Auto roles bool
    bool auto_role_vec[NROBOTS];

    QTimer *timeOwnGameBall;

    int iGOALKEEPER,iSUP_STRIKER,iSTRIKER,iDEFENDERL,iDEFENDERR;

    bool preGame;
};

#endif // MAINWINDOW_H
