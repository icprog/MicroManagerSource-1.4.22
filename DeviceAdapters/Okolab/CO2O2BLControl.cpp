///////////////////////////////////////////////////////////////////////////////
// FILE:          CO2O2BLControl.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Okolab device adapter for CO2-O2 Unit-BL 
//                
// AUTHOR:        Domenico Mastronardi @ Okolab
//                
// COPYRIGHT:     Okolab s.r.l.
//
// LICENSE:       This file is distributed under the BSD license.
//
// REVISIONS:     
//

#include "Okolab.h"
#include "CO2O2BLControl.h"
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>

const char* g_CO2O2BLControl = "CO2-O2 Unit-BL [0-10;1-18]";

using namespace std;


///////////////////////////////////////////////////////////////////////////////
// H201BLControl implementation
//

CO2O2BLControl::CO2O2BLControl() 
{
 this->product_id=12;        //    <<<<<<<<<<<<<<<< SETTARE QUESTO !!!!
 this->device_id=0;         //    <<<<<<<<<<<<<<<< SETTARE QUESTO !!!!
 this->initialized_=false;
 this->connected_=0;
 this->port_="Undefined";

 InitializeDefaultErrorMessages();

 CreateProperty(MM::g_Keyword_Name, g_CO2O2BLControl , MM::String, true);
 CreateProperty(MM::g_Keyword_Description, "Okolab CO2-O2 Unit-BL [0-10;1-18]", MM::String, true);

 CPropertyAction* pAct = new CPropertyAction (this, &CO2O2BLControl::OnPort);      
 CreateProperty("COM Port", "Undefined", MM::String, false, pAct, true);       
}


CO2O2BLControl::~CO2O2BLControl()
{
 if(initialized_) Shutdown();
}


/**
* Obtains device name.
* Required by the MM::Device API.
*/
void CO2O2BLControl::GetName(char* name) const
{
 CDeviceUtils::CopyLimitedString(name, g_CO2O2BLControl);
}


/**
* Intializes the hardware.
* Typically we access and initialize hardware at this point.
* Device properties are typically created here as well.
* Required by the MM::Device API.
*/
int CO2O2BLControl::Initialize()
{
 int ret;
 int iport;
 if(initialized_) return DEVICE_OK;

 if(!OCSRunning()) 
  { 
   LogMessage("OKO Control Server not running!",false);
   return DEVICE_COMM_HUB_MISSING;
  }

 if(sscanf(port_.c_str(),"COM%d",&iport)==1) TryConnectDevice(iport);
 else return DEVICE_INVALID_INPUT_PARAM;

 if(IsDeviceConnected(this->product_id)!=1)
  {
   return DEVICE_NOT_CONNECTED;
  }

 if(IsDeviceWorking(this->product_id)!=1)
  {
   return DEVICE_ERR;
  }

 CPropertyAction* pAct=new CPropertyAction(this, &CO2O2BLControl::OnGetCO2Conc);
 ret=CreateProperty("CO2 Concentration", "0.0", MM::Float, true, pAct);
 if(ret!=DEVICE_OK) return ret;                                                            

 CPropertyAction* pAct1=new CPropertyAction(this, &CO2O2BLControl::OnGetO2Conc);
 ret=CreateProperty("O2 Concentration", "0.0", MM::Float, true, pAct1);
 if(ret!=DEVICE_OK) return ret;                                                            

 CPropertyAction* pAct2=new CPropertyAction(this, &CO2O2BLControl::OnGetConnected);
 ret=CreateProperty("Connected", "0", MM::Integer, true, pAct2);
 if(ret!=DEVICE_OK) return ret;                           

 CPropertyAction* pAct3=new CPropertyAction(this, &CO2O2BLControl::OnGetCommPort);
 ret=CreateProperty("Serial Port", "Undefined", MM::String, true, pAct3);
 if(ret!=DEVICE_OK) return ret;                           

 CPropertyAction* pAct4=new CPropertyAction(this, &CO2O2BLControl::OnSetCommPort);
 ret=CreateProperty("Set Serial Port", "1", MM::Integer, false, pAct4, false);
 if(ret!=DEVICE_OK) return ret;                           

 CPropertyAction* pAct5=new CPropertyAction(this, &CO2O2BLControl::OnGetVersion);
 ret=CreateProperty("Version", "Undefined", MM::String, true, pAct5);
 if(ret!=DEVICE_OK) return ret;                           

 CPropertyAction* pAct6=new CPropertyAction(this, &CO2O2BLControl::OnGetCO2SetPoint);
 ret=CreateProperty("CO2 Set-Point", "0.0", MM::Float, true, pAct6);
 if(ret!=DEVICE_OK) return ret;
 
 CPropertyAction* pAct7=new CPropertyAction(this, &CO2O2BLControl::OnSetCO2SetPoint);
 ret=CreateProperty("Set CO2 Set-Point", "0.0", MM::Float, false, pAct7, false);
 if(ret!=DEVICE_OK) return ret;                           

  CPropertyAction* pAct8=new CPropertyAction(this, &CO2O2BLControl::OnGetO2SetPoint);
 ret=CreateProperty("O2 Set-Point", "0.0", MM::Float, true, pAct8);
 if(ret!=DEVICE_OK) return ret;
 
 CPropertyAction* pAct9=new CPropertyAction(this, &CO2O2BLControl::OnSetO2SetPoint);
 ret=CreateProperty("Set O2 Set-Point", "0.0", MM::Float, false, pAct9, false);
 if(ret!=DEVICE_OK) return ret;                           

 initialized_=true;

 rthread_ = new CO2O2BLControl_RefreshThread(*this);

 RefreshThread_Start();

 return DEVICE_OK;
}


/**
* Shuts down (unloads) the device.
* Required by the MM::Device API.
*/
int CO2O2BLControl::Shutdown()
{
 if(initialized_)
  {
   RefreshThread_Stop();
   delete(rthread_);
   UnconnectDevice();
  }
 initialized_=false;
 return DEVICE_OK;
}


bool CO2O2BLControl::WakeUp()
{
 return true; 
}


bool CO2O2BLControl::Busy()
{
 return false;
}


///////////////////////////////////////////////////////////////////////////////
//  Action handlers
///////////////////////////////////////////////////////////////////////////////


int CO2O2BLControl::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   pProp->Set(port_.c_str());
  }
 else if(eAct==MM::AfterSet)
       {
		if(initialized_)
		 {
          // revert
          pProp->Set(port_.c_str());
          return ERR_PORT_CHANGE_FORBIDDEN;
		 }
        pProp->Get(port_);                                                     
       }                                                                         
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetVersion(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   GetVersion();
   pProp->Set(version_.c_str());
  }
 return DEVICE_OK;
}


int CO2O2BLControl::OnGetConnected(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   long connected=0;
   if(GetConnected(connected)==DEVICE_OK) pProp->Set(connected);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetCommPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   char strComPort[21];
   GetCommPort(strComPort);
   pProp->Set(strComPort);
  }
 return DEVICE_OK;
}


int CO2O2BLControl::OnSetCommPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::AfterSet)
  {
   long cport=0;
   pProp->Get(cport);
   SetCommPort(cport);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetCO2Conc(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   double conc=0;
   if(GetCO2Conc(conc)==DEVICE_OK) pProp->Set(conc);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetO2Conc(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   double conc=0;
   if(GetO2Conc(conc)==DEVICE_OK) pProp->Set(conc);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetCO2SetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   double sp=0;
   if(GetCO2SetPoint(sp)==DEVICE_OK) pProp->Set(sp);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnSetCO2SetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::AfterSet)
  {
   double sp;
   pProp->Get(sp);
   SetCO2SetPoint(sp);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnGetO2SetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::BeforeGet)
  {
   double sp=0;
   if(GetO2SetPoint(sp)==DEVICE_OK) pProp->Set(sp);
  }
 return DEVICE_OK;     
}


int CO2O2BLControl::OnSetO2SetPoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
 if(eAct==MM::AfterSet)
  {
   double sp;
   pProp->Get(sp);
   SetO2SetPoint(sp);
  }
 return DEVICE_OK;     
}


///////////////////////////////////////////////////////////////////////////////
//  Internal API
///////////////////////////////////////////////////////////////////////////////


/*
 *  Get version of Oko Library (Okolab.cpp)
 */
int CO2O2BLControl::GetVersion()
{
 version_=OkolabDevice::version_;
 return DEVICE_OK;
}


/*
 *  Obtains ComPort (witch connects OCS to this device)
 */
int CO2O2BLControl::GetCommPort(char *strcommport)
{   
 strcpy(strcommport,"Undefined");
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 int ret=OkolabDevice::GetCommPort(strcommport);
 if(ret<=0) return DEVICE_ERR;
 return DEVICE_OK;
} 


/*
 *  Set com port (1=COM1, 2=COM2 and so on...)
 */
int CO2O2BLControl::SetCommPort(long& /*commport*/)
{
 if(!WakeUp()) return DEVICE_SERIAL_INVALID_RESPONSE;
 return DEVICE_OK;     
}



/*
 *  Obtains connection status (betweeen OCS and this device)
 */
int CO2O2BLControl::GetConnected(long& conn)
{   
 conn=0;
 connected_=0;
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 int dc=IsDeviceConnected(this->product_id);
 if(dc<0) { return DEVICE_ERR; }
 if(dc>0) { conn=1; connected_=1; return DEVICE_OK; }
 return DEVICE_NOT_CONNECTED;
} 


/*
 *  Obtains CO2 Contentration value from OCS
 */
int CO2O2BLControl::GetCO2Conc(double& conc)
{
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(0);
 int ret=GetValue(conc);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 


/*
 *  Obtains O2 Contentration value from OCS
 */
int CO2O2BLControl::GetO2Conc(double& conc)
{
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(1);
 int ret=GetValue(conc);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 



/*
 *  Obtains CO2 set-point value from OCS
 */
int CO2O2BLControl::GetCO2SetPoint(double& sp)
{   
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(0);
 int ret=OkolabDevice::GetSetPoint(sp);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 


/*
 *  Send CO2 set-point value to OCS
 */
int CO2O2BLControl::SetCO2SetPoint(double sp)
{
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(0);
 int ret=OkolabDevice::SetSetPoint(sp);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 



/*
 *  Obtains O2 set-point value from OCS
 */
int CO2O2BLControl::GetO2SetPoint(double& sp)
{   
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(1);
 int ret=OkolabDevice::GetSetPoint(sp);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 


/*
 *  Send O2 set-point value to OCS
 */
int CO2O2BLControl::SetO2SetPoint(double sp)
{
 if(!initialized_) return DEVICE_NOT_CONNECTED;
 this->SetDeviceId(1);
 int ret=OkolabDevice::SetSetPoint(sp);
 if(ret<0) DEVICE_ERR;
 return DEVICE_OK;                                                         
} 


/*
 *  Test serial communication between OSC and device 
 */
MM::DeviceDetectionStatus CO2O2BLControl::DetectDevice(void)
{
 if(initialized_) return MM::CanCommunicate;
 return Detect();
}


int CO2O2BLControl::IsConnected()
{
 return this->connected_;
}


void CO2O2BLControl::UpdateGui()
{
 this->OnPropertiesChanged();
}


void CO2O2BLControl::UpdatePropertyGui(char *PropName, char *PropVal)
{
 this->OnPropertyChanged(PropName,PropVal);
}



void CO2O2BLControl::RefreshThread_Start()
{
 rthread_->Start();
}


void CO2O2BLControl::RefreshThread_Stop()
{
 rthread_->Stop();
}



///////////////////////////////////////////////////////////////////////////////
// Refresh Thread Class
///////////////////////////////////////////////////////////////////////////////


CO2O2BLControl_RefreshThread::CO2O2BLControl_RefreshThread(CO2O2BLControl &oDevice) :
   stop_(true), okoDevice_(oDevice)
{
 sleepmillis_=2000;
};


CO2O2BLControl_RefreshThread::~CO2O2BLControl_RefreshThread()
{
 Stop();
 wait();
}


int CO2O2BLControl_RefreshThread::svc() 
{
 char strVal[20]; 
 double v=0;
 while(!stop_)
  {
   if(okoDevice_.IsConnected()==1)
    {
     okoDevice_.SetDeviceId(0);
     int ret=okoDevice_.GetValue(v);
     snprintf(strVal,20,"%.02f",v);
     okoDevice_.UpdatePropertyGui("CO2 Concentration",strVal); 
     CDeviceUtils::SleepMs(500);
     okoDevice_.SetDeviceId(1);
     ret=okoDevice_.GetValue(v);
     snprintf(strVal,20,"%.02f",v);
     okoDevice_.UpdatePropertyGui("O2 Concentration",strVal);  
    }
/*
FILE *fp;
fp=fopen("log_test_thread.txt","a");
fprintf(fp,"thread running; val=%f\n",v);
fclose(fp);
*/
   CDeviceUtils::SleepMs(sleepmillis_);
  }
 return DEVICE_OK;
}


void CO2O2BLControl_RefreshThread::Start()
{
 if(stop_) { stop_=false; activate(); }
}
