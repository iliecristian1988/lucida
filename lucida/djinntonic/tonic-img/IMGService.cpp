// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <map>
#include <glog/logging.h>

#include "opencv2/opencv.hpp"
#include "boost/program_options.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/algorithm/string/replace.hpp"
#include "boost/lexical_cast.hpp"

#include "caffe/caffe.hpp"
#include "../tonic-common/src/tonic.h"
#include "../tonic-common/src/serialize.h"
#include "common-img/src/align.h"

#include "../gen-cpp/LucidaService.h"
#include "../gen-cpp/lucidaservice_constants.h"
#include "../gen-cpp/lucidaservice_types.h"
#include "../gen-cpp/lucidatypes_constants.h"
#include "../gen-cpp/lucidatypes_types.h"
#include "../tonic-common/src/ServiceTypes.h"
#include "../tonic-common/src/commons.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace std;
using namespace cv;

namespace po = boost::program_options;
struct timeval tv1, tv2;
ServiceTypes svt;

class LucidaServiceHandler : virtual public LucidaServiceIf
{
 public:
  LucidaServiceHandler() {
    this->SERVICE_NAME      = svt.IMP_SERVICE;
    this->SERVICE_IP        = "localhost";
    this->SERVICE_PORT      = 8080;
    this->DJINN_IP          = "localhost";
    this->DJINN_PORT        = 5000;
    app.task                = "imc";
    align                   = true;
    haar                    = "common-img/data/haar.xml";
    flandmark               = "common-img/data/flandmark.dat";
  }

  void init(po::variables_map vm) {
    this->SERVICE_IP        = vm["svip"].as<string>();
    this->SERVICE_PORT      = vm["port"].as<int>();
    this->DJINN_IP          = vm["djinnip"].as<string>();
    this->DJINN_PORT        = vm["djinnp"].as<int>();
  }

  void create(const std::string& LUCID, const  ::QuerySpec& spec) {
    // Your implementation goes here
    printf("create\n");
    // create model using LUCID
    // finetune existing model
  }

  void learn(const std::string& LUCID, const  ::QuerySpec& knowledge) {
    // Your implementation goes here
    printf("learn\n");
  }

  void infer(std::string& _return, const std::string& LUCID, const  ::QuerySpec& query) {
    time_t rawtime;
    time(&rawtime);
    LOG(INFO) << "receiving image query at " << ctime(&rawtime);

    int64_t start_time, end_time;

    for (int i = 0; i < query.content.size(); i++){
      struct timeval now;
      gettimeofday(&now, NULL);
      start_time = (now.tv_sec*1E6+now.tv_usec) / 1000;
      string str = exec_img(LUCID, query.content[i]);
      _return += str;
      gettimeofday(&now, 0);
      end_time = (now.tv_sec*1E6+now.tv_usec) / 1000;
    }
  }

private:
  TonicSuiteApp app;
  string SERVICE_NAME;
  string SERVICE_IP;
  int SERVICE_PORT;
  string DJINN_IP;
  int DJINN_PORT;
  bool align;
  string flandmark;
  string haar;

  //parse command line arguments from the client side
  void parse_tags(const std::vector<string> &tags){
    for(int i = 0; i < tags.size(); ++i){
      if (tags[i] == "--task"           || tags[i] == "-t")
        app.task        = tags[i + 1];
      else if (tags[i] == "--align"     || tags[i] == "-l")
        align           = (tags[i + 1] == "1")? true : false;
      else if (tags[i] == "--haar"      || tags[i] == "-a")
        haar        = tags[i + 1];
      else if (tags[i] == "--flandmark" || tags[i] == "-f")
        flandmark   = tags[i + 1];
    }
  }

  void call_resize(const std::string& LUCID, Mat &img, string filename, string imgData){
    // TODO(JAH): this isn't hooked in well. the IP and PORT are hardcoded.
    // Thought needs to go into the scaling of more services along the path of
    // an image query. if another service is added, shouldn't be another
    // command arg, should figure something better

    //cause preprocess to be called after this function
    if(app.task == "face"){
       align = true;
    }
    //resize for IMC, DIG
    else{
      try{
        TClient tClient;
        LucidaServiceClient * resizeClient = NULL;
        resizeClient = tClient.creatLucidaClient("localhost", 5001);

        //set up the queryspec
        QuerySpec query;
        QueryInput impQueryInput;

        string type = svt.SERVICE_INPUT_IMAGE;
        impQueryInput.__set_type(type);

        vector<string> tags;
        vector<string> data;

        //specific sizes for each task
        string lw_size;
        if (app.task == "imc") lw_size = "227";
        else if (app.task == "face") lw_size = "152";
        else if (app.task == "dig") lw_size = "28";

        tags.push_back(lw_size);
        tags.push_back(lw_size);
        tags.push_back(filename);
        
        impQueryInput.__set_tags(tags);
        LOG(INFO) << "Successfully set tags";
        
        data.push_back(imgData);
        impQueryInput.__set_data(data);
        LOG(INFO) << "Successfully set input";

        query.content.push_back(impQueryInput);

        string outstring;
        LOG(INFO) << "Resizing... "<< endl;
     
        resizeClient->infer(outstring, LUCID, query);

        stringstream temp;
        temp << outstring;
        string temp1;
        img = deserialize(temp, temp1);
        
      } catch (TException& tx) {
        LOG(ERROR) << "Could not talk to resize" << endl;    
      }
    }
  }

  string call_djinn(const std::string& LUCID, stringstream &ret, string & s){
    string outstring;

    try {
      TClient tClient;
      LucidaServiceClient * imgClient = NULL;
      imgClient = tClient.creatLucidaClient(DJINN_IP, DJINN_PORT);

      //set up the data query
      QuerySpec query;
      query.__set_name(svt.SERVICE_INPUT_IMAGE);

      LOG(INFO) << "Setting tags";

      QueryInput djinnQueryInput;
      vector<string> tags;
      string req_name = app.task;
      int length = app.pl.num * app.pl.size;
      stringstream ss;
      ss << length;
      string str = ss.str();
      tags.push_back(req_name);
      tags.push_back(str);
      djinnQueryInput.__set_tags(tags);

      LOG(INFO) << "Request Name: " << req_name;
      LOG(INFO) << "Length: " << app.pl.num * app.pl.size;
      LOG(INFO) << "Setting input data...";
        
      vector<string> dataVec;
      dataVec.push_back(s);

      djinnQueryInput.__set_data(dataVec);

      query.content.push_back(djinnQueryInput);
      
      imgClient->infer(outstring, LUCID, query);
      return outstring;

    } catch (TException& tx) {
        LOG(ERROR) << "Could not talk to Djinn" << endl;    
    }
  }

  string exec_img(const std::string& LUCID, const QueryInput &q_in){
    parse_tags(q_in.tags);
    app.pl.size = 0;
    
    // hardcoded for AlexNet
    strcpy(app.pl.req_name, app.task.c_str());
    if (app.task == "imc") app.pl.size = 3 * 227 * 227;
    // hardcoded for DeepFace
    else if (app.task == "face")
      app.pl.size = 3 * 152 * 152;
    // hardcoded for Mnist
    else if (app.task == "dig")
      app.pl.size = 1 * 28 * 28;
    else
      LOG(FATAL) << "Unrecognized task.\n";

    // read in images
    // cmt: using map, cant use duplicate names for images
    // change to other structure (vector) if you want to send the same exact
    // filename multiple times

    vector<pair<string, Mat> > imgs;
    app.pl.num = 0;
    int j = 0;
    for (int i = 0; i < q_in.data.size(); ++i) {
      LOG(INFO) << "Reading images...";
      stringstream ss;
      ss << q_in.data[i];
     
      string filename;
      Mat img = deserialize(ss, filename);
      
      //Convert to grayscale for dig
      if (app.task == "dig"){
        Mat gs_bgr(img.size(), CV_8UC1);
        cvtColor(img, gs_bgr, CV_BGR2GRAY);
        img = gs_bgr;
      }
      
      // Resize if neccessary
      if (img.channels() * img.rows * img.cols != app.pl.size){
        LOG(ERROR) << "resizing " << filename << " to correct dimensions.\n";
        call_resize(LUCID, img, filename, q_in.data[i]);
      } 
      //aligns and resizes image for FACE
      if(app.task == "face" && align){
        LOG(INFO) << "aligning " << filename;
        preprocess(img, flandmark, haar);
        // comment in to save + view aligned image
        // imwrite(filename+"_a", img);
      }

      imgs.push_back(make_pair(filename, img));
      ++app.pl.num;  
    }

    if (app.pl.num < 1) LOG(FATAL) << "No images read!";
    
    string s;
    // prepare data into array
    vector<pair<string, Mat> >::iterator it;
    app.pl.data = (float*)malloc(app.pl.num * app.pl.size * sizeof(float));
    float* preds = (float*)malloc(app.pl.num * sizeof(float));
    int img_count = 0;
    for (it = imgs.begin(); it != imgs.end(); ++it) {
      int pix_count = 0;
      for (int c = 0; c < it->second.channels(); ++c) {
        for (int i = 0; i < it->second.rows; ++i) {
          for (int j = 0; j < it->second.cols; ++j) {
            Vec3b pix = it->second.at<Vec3b>(i, j);
            float* p = (float*)(app.pl.data);
            p[img_count * app.pl.size + pix_count] = pix[c];
            //setting up a string for queryInput            
            s += std::to_string(pix[c]) + " ";
            ++pix_count;
          }
        }
      }
      ++img_count;
    }
    
    stringstream ret;
    string outstring = call_djinn(LUCID, ret, s);

    istringstream iss(outstring);
    string predString;
     
    for (it = imgs.begin(); it != imgs.end(); it++) {
      iss >> predString;
      ret << "Image: " << it->first << " class: " << predString << "\n";
      LOG(INFO) << "Image: " << it->first
                << " class: " << predString << endl;
    }
   
    free(app.pl.data);
    free(preds);

    return ret.str();
  }

};

//parse command line arguments from server side
po::variables_map parse_opts(int ac, char** av)
{
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "Produce help message")
      ("port,p", po::value<int>()->default_value(8080),
          "Service port number")
      ("svip,s", po::value<string>()->default_value("localhost"),
          "Service IP address")
      ("djinnp,d", po::value<int>()->default_value(5000),
          "Djinn Port number")
      ("djinnip,i", po::value<string>()->default_value("localhost"),
          "Djinn ip address")
        ;

  po::variables_map vm;
  po::store(po::parse_command_line(ac, av, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    exit(1);
  }
  return vm;
}

int main(int argc, char **argv)
{
  po::variables_map vm = parse_opts(argc, argv);

  LucidaServiceHandler *IMGService = new LucidaServiceHandler();

  boost::shared_ptr<LucidaServiceHandler> handler(IMGService);
  boost::shared_ptr<TProcessor> processor(new LucidaServiceProcessor(handler));
    
  TServers tServer;
  thread thrift_server;
    
  tServer.launchSingleThreadThriftServer(vm["port"].as<int>(), processor, thrift_server);
  IMGService->init(vm);
  LOG(INFO) << "Starting the Tonic IMG service at "
            << vm["svip"].as<string>() << ":"
            << vm["port"].as<int>() 
            << endl;

  thrift_server.join();

  return 0;
}
