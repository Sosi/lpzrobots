/***************************************************************************
 *   Copyright (C) 2005 by Robot Group Leipzig                             *
 *    martius@informatik.uni-leipzig.de                                    *
 *    fhesse@informatik.uni-leipzig.de                                     *
 *    der@informatik.uni-leipzig.de                                        *
 *    frankguettler@gmx.de                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   $Log$
 *   Revision 1.1.2.5  2006-05-09 08:46:37  robot3
 *   getSensors() and getMotors() modified
 *
 *   Revision 1.1.2.4  2006/05/09 04:24:34  robot5
 *   *** empty log message ***
 *
 *   Revision 1.1.2.3  2006/04/25 09:03:52  robot3
 *   caterpillar is now represented by a box
 *
 *   Revision 1.1.2.2  2006/04/11 13:26:46  robot3
 *   caterpillar is using now methods from schlangeservo2
 *
 *   Revision 1.1.2.1  2006/04/11 08:09:47  robot3
 *   first version
 *
 *
 ***************************************************************************/

#include "caterpillar.h"

namespace lpzrobots {

  CaterPillar::CaterPillar ( const OdeHandle& odeHandle, const OsgHandle& osgHandle,
			     const CaterPillarConf& conf, const char* n) 
    : DefaultCaterPillar(odeHandle, osgHandle, conf, n)
  {
    Configurable::insertCVSInfo(name, "$RCSfile$", 
				"$Revision$");
  }
	
  CaterPillar::~CaterPillar() { }
	

  /**
   *Reads the actual motor commands from an array, and sets all motors (forces) of the snake to this values.
   *It is a linear allocation.
   *@param motors pointer to the array, motor values are scaled to [-1,1] 
   *@param motornumber length of the motor array
   **/
  void CaterPillar::setMotors (const motor* motors, int motornumber)
  {
    assert(created);
    int len = min(motornumber, getMotorNumber())/2;
    // controller output as torques 
    for (int i = 0; (i < len) && (universalServos.size()); i++){
      universalServos[i]->set(motors[2*i], motors[2*i+1]);
    }
    for (int i = 0; (i < len) && (sliderServos.size()); i++){
      sliderServos[i]->set(motors[i+universalServos.size()*2]);
    }
  }

  /**
   *Writes the sensor values to an array in the memory.
   *@param sensor* pointer to the arrays

   *@param sensornumber length of the sensor array
   *@return number of actually written sensors
   **/
  int CaterPillar::getSensors (sensor* sensors, int sensornumber)
  {
    assert(created);
    int len = min(sensornumber, getSensorNumber());
    // first get the universalServos (2 sensors each!)
    for (int n = 0; (n < len/2) && (n<universalServos.size()); n++) {
      sensors[2*n] = universalServos[n]->get1();
      sensors[2*n+1] = universalServos[n]->get2();
    }
    for (int n = universalServos.size()*2;n< len; n++) {
      sensors[n] = sliderServos[n-universalServos.size()*2]->get()*0.2;
    }

    return 2*len;
  }


  /** creates vehicle at desired position 
      @param pos struct Position with desired position
  */
  void CaterPillar::create(const osg::Matrix& pose){
    DefaultCaterPillar::create(pose);
    
    //*****************joint definition***********
    for (int n = 0; n < conf.segmNumber-1; n++) {

      const Pos& p1(objects[n]->getPosition());
      const Pos& p2(objects[n+1]->getPosition());

      // normal servos creating //
      UniversalJoint* j = new UniversalJoint(objects[n], objects[n+1],
					     (p1 + p2)/2,
					     Axis(0,0,1)* pose, Axis(0,1,0)* pose);
      j->init(odeHandle, osgHandle, true, conf.segmDia/2 * 1.02);

      // setting stops at universal joints
      j->setParam(dParamLoStop, -conf.jointLimit*1.5);
      j->setParam(dParamHiStop,  conf.jointLimit*1.5);
      joints.push_back(j);

      UniversalServo* servo =  new UniversalServo(j, -conf.jointLimit, conf.jointLimit, conf.motorPower,
					          -conf.jointLimit, conf.jointLimit, conf.motorPower);
      universalServos.push_back(servo);
      frictionmotors.push_back(new AngularMotor2Axis(odeHandle, j, conf.frictionJoint, conf.frictionJoint));
      // end of normal servos //

    }
      // new slider joints //
      SliderJoint *s=new SliderJoint(objects[0], objects[conf.segmNumber-1], osg::Vec3((0), (conf.segmDia), (0)), Axis(1,0,0)* pose);
      s->init(odeHandle, osgHandle);

      s->setParam(dParamLoStop, -1.1*conf.segmDia);
      s->setParam(dParamHiStop, 1.1*conf.segmDia);
      s->setParam(dParamStopCFM, 0.1);
      s->setParam(dParamStopERP, 0.9);
      s->setParam(dParamCFM, 0.001);

      joints.push_back(s);
      SliderServo *ss = new SliderServo(s,-conf.segmDia,conf.segmDia,conf.segmMass);
      sliderServos.push_back(ss);
      // end of new slider joints //
  }

  bool CaterPillar::setParam(const paramkey& key, paramval val){
    bool rv = DefaultCaterPillar::setParam(key, val);
    for (vector<UniversalServo*>::iterator i = universalServos.begin(); i!= universalServos.end(); i++){
      if(*i) (*i)->setPower(conf.motorPower, conf.motorPower);
    }
    return rv;
  }

  /** destroys vehicle and space
   */
  void CaterPillar::destroy(){
    if (created){
      DefaultCaterPillar::destroy();  
      for (vector<UniversalServo*>::iterator i = universalServos.begin(); i!= universalServos.end(); i++){
	if(*i) delete *i;
      }
      universalServos.clear();
    }
  }

}