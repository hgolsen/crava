#include <fstream>
#include <iostream>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include "lib/global_def.h"
#include "lib/lib_misc.h"

#include "nrlib/iotools/logkit.hpp"
#include "nrlib/iotools/stringtools.hpp"
#include "nrlib/segy/segy.hpp"

#include "src/inputfiles.h"
#include "src/modelsettings.h"
#include "src/xmlmodelfile.h"
#include "src/vario.h"
#include "src/definitions.h"
#include "src/fftgrid.h"
#include "src/vario.h"
#include "lib/utils.h"

XmlModelFile::XmlModelFile(const char * fileName)
{
  modelSettings_ = new ModelSettings();
  inputFiles_    = new InputFiles();
  failed_        = false;

  std::ifstream file(fileName);

  if (!file) {
    LogKit::LogFormatted(LogKit::ERROR,"Error: Could not open file %s for reading.\n", fileName);
    exit(1);
  }

  //Remove all comments, since this convention is outside xml.
  std::string line;
  std::string active;
  std::string clean;
  while (std::getline(file,line))
  {
    active = line.substr(0, line.find_first_of("#"));
    clean = clean+active+"\n";
  }
  file.close();

  TiXmlDocument doc;
  doc.Parse(clean.c_str());

  if (doc.Error() == true) {
    LogKit::LogFormatted(LogKit::ERROR,"Error: Reading of xml-file %s failed. %s Line %d, column %d\n", 
      fileName, doc.ErrorDesc(), doc.ErrorRow(), doc.ErrorCol());
    failed_ = true;
  }
  else {
    std::string errTxt = "";
    if(parseCrava(&doc, errTxt) == false)
      errTxt = "'"+std::string(fileName)+"' is not a crava model file (lacks the <crava> keyword.\n";

    std::vector<std::string> legalCommands(1);
    checkForJunk(&doc, errTxt, legalCommands);

    checkConsistency(errTxt);


    if(errTxt != "") {
      Utils::writeHeader("Error(s) detected when parsing model file");
      LogKit::LogMessage(LogKit::ERROR, "\n"+errTxt);
      failed_ = true;
    }
  }
}

XmlModelFile::~XmlModelFile()
{
  //Both modelSettings and inputFiles are taken out and deleted outside.
}

bool
XmlModelFile::parseCrava(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("crava");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(5);
  legalCommands[0] = "actions";
  legalCommands[1] = "project-settings";
  legalCommands[2] = "survey";
  legalCommands[3] = "well-data";
  legalCommands[4] = "prior-model";

  parseWellData(root, errTxt);
  parseSurvey(root, errTxt);
  parseProjectSettings(root, errTxt);
  parsePriorModel(root, errTxt);
  parseActions(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}

  
bool
XmlModelFile::parseActions(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("actions");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(3);
  legalCommands[0]="mode";
  legalCommands[1]="inversion-settings";
  legalCommands[2]="estimation-settings";

  std::string mode;
  if(parseValue(root, "mode", mode, errTxt) == false)
    errTxt += "Command <mode> must be given under command <"+
      root->ValueStr()+">"+lineColumnText(root)+".\n";
  else {
    if(mode == "forward")
      modelSettings_->setGenerateSeismic(true);
    else if(mode == "estimation")
      modelSettings_->setEstimationMode(true);
    else if(mode != "inversion")
      errTxt += "String after <mode> must be either <inversion>, <estimation> or <forward>, found <"+
        mode+"> under command <"+root->ValueStr()+">"+lineColumnText(root)+".\n";
  }

  parseInversionSettings(root, errTxt);

  parseEstimationSettings(root, errTxt);
  
  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseInversionSettings(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("inversion-settings");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="prediction";
  legalCommands[1]="simulation";
  legalCommands[2]="condition-to-wells";
  legalCommands[3]="facies-probabilities";

  bool value;
  if(parseBool(root, "prediction", value, errTxt) == true)
    modelSettings_->setWritePrediction(value);
  
  parseSimulation(root, errTxt);

  if(parseBool(root, "condition-to-wells", value, errTxt) == true) {
    if(value == true) {
      if(modelSettings_->getKrigingParameter() == 0)
        modelSettings_->setKrigingParameter(250); //Default value.
    }
    else
      modelSettings_->setKrigingParameter(-1);
  }

  std::string facprob;
  if(parseValue(root, "facies-probabilities", facprob, errTxt) == true) {
    int flag = 0;
    if(modelSettings_->getDefaultGridOutputInd() == false)
      flag = modelSettings_->getGridOutputFlag();
    if(facprob == "absolute")
      flag = (flag | ModelSettings::FACIESPROB);
    else if(facprob == "relative")
      flag = (flag | ModelSettings::FACIESPROBRELATIVE);
    if(flag != 0) {
      modelSettings_->setGridOutputFlag(flag);
      modelSettings_->setDefaultGridOutputInd(false);
      modelSettings_->setEstimateFaciesProb(true);
    }
  }

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseSimulation(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("simulation");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="seed";
  legalCommands[1]="seed-file";
  legalCommands[2]="number-of-simulations";

  int seed;
  bool seedGiven = parseValue(root, "seed", seed, errTxt);
  if(seedGiven == true)
    modelSettings_->setSeed(seed);

  std::string filename;
  if(parseFileName(root, "seed-file", filename, errTxt) == true) {
    if(seedGiven == true)
      errTxt += "Both seed and seed file given in command <"+
        root->ValueStr()+"> "+lineColumnText(root)+".\n";
  }

  int value = 1;
  parseValue(root, "number-of-simulations", value, errTxt);
  modelSettings_->setNumberOfSimulations(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseEstimationSettings(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("estimation-settings");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="estimate-background";
  legalCommands[1]="estimate-correlations";
  legalCommands[2]="estimate-wavelet-or-noise";

  bool estimateBG = true;
  parseBool(root,"estimate-background", estimateBG, errTxt);
  modelSettings_->setEstimateBackground(estimateBG);

  bool estimateCorr = true;
  parseBool(root,"estimate-correlations", estimateCorr, errTxt);
  modelSettings_->setEstimateCorrelations(estimateCorr);

  bool estimateWN = true;
  parseBool(root,"estimate-wavelet-or-noise", estimateWN, errTxt);
  modelSettings_->setEstimateWaveletNoise(estimateWN);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWellData(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("well-data");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(7);
  legalCommands[0]="log-names";
  legalCommands[1]="well";
  legalCommands[2]="high-cut-seismic-resolution";
  legalCommands[3]="allowed-parameter-values";
  legalCommands[4]="maximum-deviation-angle";
  legalCommands[5]="maximum-rank-correlation";
  legalCommands[6]="maximum-merge-distance";

  parseLogNames(root, errTxt);

  int nWells = 0;
  while(parseWell(root, errTxt) == true)
    nWells++;
  modelSettings_->setNumberOfWells(nWells);

  float value;
  if(parseValue(root, "high-cut-seismic-resolution", value, errTxt) == true)
    modelSettings_->setHighCut(value);

  parseAllowedParameterValues(root, errTxt);

  if(parseValue(root, "maximum-deviation-angle", value, errTxt) == true)
    modelSettings_->setMaxDevAngle(value);

  if(parseValue(root, "maximum-rank-correlation", value, errTxt) == true)
    modelSettings_->setMaxRankCorr(value);

  if(parseValue(root, "maximum-merge-distance", value, errTxt) == true)
    modelSettings_->setMaxMergeDist(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseLogNames(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("log-names");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(7);
  legalCommands[0]="time";
  legalCommands[1]="vp";
  legalCommands[2]="dt";
  legalCommands[3]="vs";
  legalCommands[4]="dts";
  legalCommands[5]="density";
  legalCommands[6]="facies";

  std::string value;
  if(parseValue(root, "time", value, errTxt) == true)
    modelSettings_->setLogName(0, value);

  bool vp = parseValue(root, "vp", value, errTxt);
  if(vp == true) {
    modelSettings_->setLogName(1, value);
    modelSettings_->setInverseVelocity(0, false);
  }
  if(parseValue(root, "dt", value, errTxt) == true) {
    if(vp == true)
      errTxt += "Both vp and dt given as logs in command <"
        +root->ValueStr()+"> "+lineColumnText(root)+".\n";
    else {
      modelSettings_->setLogName(1, value);
      modelSettings_->setInverseVelocity(0, true);
    }
  }

  bool vs = parseValue(root, "vs", value, errTxt);
  if(vp == true) {
    modelSettings_->setLogName(3, value);
    modelSettings_->setInverseVelocity(1, false);
  }
  if(parseValue(root, "dts", value, errTxt) == true) {
    if(vs == true)
      errTxt += "Both vs and dts given as logs in command <"
        +root->ValueStr()+"> "+lineColumnText(root)+".\n";
    else {
      modelSettings_->setLogName(3, value);
      modelSettings_->setInverseVelocity(1, true);
    }
  }

  if(parseValue(root, "density", value, errTxt) == true)
    modelSettings_->setLogName(2, value);
  if(parseValue(root, "facies", value, errTxt) == true) {
    modelSettings_->setLogName(4, value);
    modelSettings_->setFaciesLogGiven(true);
  }
  
  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWell(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("well");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="file-name";
  legalCommands[1]="use-for-wavelet-estimation";
  legalCommands[2]="use-for-background-trend";
  legalCommands[3]="use-for-facies-probabilities";

  std::string tmpErr = "";
  std::string value;
  if(parseValue(root, "file-name", value, tmpErr) == true) {
    inputFiles_->addWellFile(value);
  }
  else
    inputFiles_->addWellFile(""); //Dummy to keep tables balanced.

  bool use;
  if(parseBool(root, "use-for-wavelet-estimation", use, tmpErr) == true && use == false)
    modelSettings_->addIndicatorWavelet(0);
  else
    modelSettings_->addIndicatorWavelet(1);

  if(parseBool(root, "use-for-background-trend", use, tmpErr) == true && use == false)
    modelSettings_->addIndicatorBGTrend(0);
  else
    modelSettings_->addIndicatorBGTrend(1);

  if(parseBool(root, "use-for-facies-probabilities", use, tmpErr) == true && use == false)
    modelSettings_->addIndicatorFacies(0);
  else
    modelSettings_->addIndicatorFacies(1);

  checkForJunk(root, errTxt, legalCommands, true); //Allow duplicates

  errTxt += tmpErr;
  return(true);
}

bool
XmlModelFile::parseAllowedParameterValues(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("allowed-parameter-values");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(12);
  legalCommands[0]="minimum-vp";
  legalCommands[1]="maximum-vp";
  legalCommands[2]="minimum-vs";
  legalCommands[3]="maximum-vs";
  legalCommands[4]="minimum-density";
  legalCommands[5]="maximum-density";
  legalCommands[6]="minimum-variance-vp";
  legalCommands[7]="maximum-variance-vp";
  legalCommands[8]="minimum-variance-vs";
  legalCommands[9]="maximum-variance-vs";
  legalCommands[10]="minimum-variance-density";
  legalCommands[11]="maximum-variance-density";

  float value;
  if(parseValue(root, "minimum-vp", value, errTxt) == true)
    modelSettings_->setAlphaMin(value);
  if(parseValue(root, "maximum-vp", value, errTxt) == true)
    modelSettings_->setAlphaMax(value);
  if(parseValue(root, "minimum-vs", value, errTxt) == true)
    modelSettings_->setBetaMin(value);
  if(parseValue(root, "maximum-vs", value, errTxt) == true)
    modelSettings_->setBetaMax(value);
  if(parseValue(root, "minimum-density", value, errTxt) == true)
    modelSettings_->setRhoMin(value);
  if(parseValue(root, "maximum-density", value, errTxt) == true)
    modelSettings_->setRhoMax(value);

  if(parseValue(root, "minimum-variance-vp", value, errTxt) == true)
    modelSettings_->setVarAlphaMin(value);
  if(parseValue(root, "maximum-variance-vp", value, errTxt) == true)
    modelSettings_->setVarAlphaMax(value);
  if(parseValue(root, "minimum-variance-vs", value, errTxt) == true)
    modelSettings_->setVarBetaMin(value);
  if(parseValue(root, "maximum-variance-vs", value, errTxt) == true)
    modelSettings_->setVarBetaMax(value);
  if(parseValue(root, "minimum-variance-density", value, errTxt) == true)
    modelSettings_->setVarRhoMin(value);
  if(parseValue(root, "maximum-variance-density", value, errTxt) == true)
    modelSettings_->setVarRhoMax(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseSurvey(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("survey");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(5);
  legalCommands[0]="angular-correlation";
  legalCommands[1]="segy-start-time";
  legalCommands[2]="direct-files";
  legalCommands[3]="angle-gather";
  legalCommands[4]="wavelet-estimation-interval";

  Vario * vario = NULL;
  if(parseVariogram(root, "angular-correlation", vario, errTxt) == true) {
    if (vario != NULL) {
      vario->convertRangesFromDegToRad();
      modelSettings_->setAngularCorr(vario);
    }
  }

  float value;
  if(parseValue(root, "segy-start-time", value, errTxt) == true)
    modelSettings_->setSegyOffset(value);

  bool direct = false;
  parseBool(root, "direct-files", direct, errTxt);
  modelSettings_->setDirectSeisInput(direct);

  while(parseAngleGather(root, errTxt) == true);

  if(modelSettings_->getNumberOfAngles() == 0)
    errTxt +=  "Need at least one angle gather in command <"+root->ValueStr()+">"+
    lineColumnText(root)+".\n";

  parseWaveletEstimationInterval(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseAngleGather(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("angle-gather");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(7);
  legalCommands[0]="offset-angle";
  legalCommands[1]="seismic-data";
  legalCommands[2]="wavelet";
  legalCommands[3]="match-energies";
  legalCommands[4]="signal-to-noise-ratio";
  legalCommands[5]="local-noise-scaled";
  legalCommands[6]="estimate-local-noise";

  float value;
  if(parseValue(root, "offset-angle", value, errTxt) == true)
    modelSettings_->addAngle(value*float(M_PI/180));
  else
    errTxt += "Need offset angle for gather"+lineColumnText(root)+".\n";

  if(parseSeismicData(root, errTxt) == false) {
    //Go for defaults. Assume forward model (check later)
    inputFiles_->addSeismicFile("");
    modelSettings_->addSeismicType(ModelSettings::STANDARDSEIS);
  }

  if(parseWavelet(root, errTxt) == false) {
    inputFiles_->addWaveletFile("");
    modelSettings_->addEstimateWavelet(true);
    modelSettings_->addWaveletScale(RMISSING);
    modelSettings_->addEstimateGlobalWaveletScale(false); 
    inputFiles_->addShiftFile("");
    inputFiles_->addScaleFile("");
    modelSettings_->addEstimateLocalShift(false);
    modelSettings_->addEstimateLocalScale(false);
  }

  bool bVal = false;
  if(parseBool(root, "match-energies", bVal, errTxt) == true)
    modelSettings_->addMatchEnergies(bVal);
  else
    modelSettings_->addMatchEnergies(false);

  if(parseValue(root, "signal-to-noise-ratio", value, errTxt) == true) {
    modelSettings_->addEstimateSNRatio(false);
    modelSettings_->addSNRatio(value);
  }
  else {
    modelSettings_->addEstimateSNRatio(true);
    modelSettings_->addSNRatio(RMISSING);
  }
   std::string fileName; 
  bool localNoiseGiven = parseFileName(root, "local-noise-scaled", fileName, errTxt);
  if(localNoiseGiven)
    inputFiles_->addNoiseFile(fileName);
  else
    inputFiles_->addNoiseFile("");

  bool estimate;
  std::string tmpErr;
  if(parseBool(root, "estimate-local-noise",estimate,tmpErr) == false || tmpErr != "") {
    modelSettings_->addEstimateLocalNoise(false);
    errTxt += tmpErr;
  }
  else 
    if(estimate == true && localNoiseGiven == true) {
      errTxt = errTxt +"Error: Can not give both file and ask for estimation of local noise"+
        lineColumnText(node);
      modelSettings_->addEstimateLocalNoise(false);
    }
    else
      modelSettings_->addEstimateLocalNoise(estimate);

  checkForJunk(root, errTxt, legalCommands, true);
  return(true);
}


bool
XmlModelFile::parseSeismicData(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("seismic-data");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(4);
  legalCommands[0]="file-name";
  legalCommands[1]="start-time";
  legalCommands[2]="segy-format";
  legalCommands[3]="type";

  std::string value;
  if(parseFileName(root, "file-name", value, errTxt) == true)
    inputFiles_->addSeismicFile(value);
  else
    inputFiles_->addSeismicFile(""); //Keeping tables balanced.

  if(parseValue(root, "type", value, errTxt) == true) {
    if(value == "pp") 
      modelSettings_->addSeismicType(ModelSettings::STANDARDSEIS);
    else if(value == "ps")
      modelSettings_->addSeismicType(ModelSettings::PSSEIS);
    else
      errTxt += "Only 'pp' and 'ps' are valid seismic types, found '"+value+"' on line "
        +NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
  }
  else
    modelSettings_->addSeismicType(ModelSettings::STANDARDSEIS);

  float fVal;
  if(parseValue(root, "start-time", fVal, errTxt) == true)
    modelSettings_->addLocalSegyOffset(fVal);
  else
    modelSettings_->addLocalSegyOffset(-1);

  TraceHeaderFormat * thf = NULL;
  if(parseTraceHeaderFormat(root, "segy-format", thf, errTxt) == true) {
    modelSettings_->addTraceHeaderFormat(thf);
  }
  else
    modelSettings_->addTraceHeaderFormat(NULL);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWavelet(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("wavelet");
  if(root == 0)
  {
    return(false);
  }

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="file-name";
  legalCommands[1]="scale";
  legalCommands[2]="estimate-scale";
  legalCommands[3]="local-wavelet";

  std::string value;
  if(parseFileName(root, "file-name", value, errTxt) == true) {
    inputFiles_->addWaveletFile(value);
    modelSettings_->addEstimateWavelet(false);
  }
  else {
    inputFiles_->addWaveletFile(""); //Keeping tables balanced.
    modelSettings_->addEstimateWavelet(true);
  }
  
  float scale;
  bool scaleGiven = false;
  if(parseValue(root, "scale", scale, errTxt) == true)
  {
    scaleGiven = true;
    modelSettings_->addWaveletScale(scale);
    modelSettings_->addEstimateGlobalWaveletScale(false);
  }
  bool estimate;
  std::string tmpErr;
 
  if(parseBool(root, "estimate-scale",estimate,tmpErr) == false || tmpErr != "") {
   if(scaleGiven==false) // no commands given
   {
    modelSettings_->addWaveletScale(1);
    modelSettings_->addEstimateGlobalWaveletScale(false);
   }
    errTxt += tmpErr;
  }
  else 
    if(estimate == true && scaleGiven == true) {
      errTxt = errTxt +"Error: Can not give both value and ask for estimation of global wavelet scale"+
        lineColumnText(node);
     // modelSettings_->addEstimateGlobalWaveletScale(false);
    }
    else if(estimate==true && scaleGiven==false)// estimate==true eller false, scaleGiven==false
    {
      modelSettings_->addEstimateGlobalWaveletScale(true);
      modelSettings_->addWaveletScale(1);
    } 
    else if(estimate==false && scaleGiven==false)// estimate given as no, no scale command given
    {
      modelSettings_->addWaveletScale(1);
      modelSettings_->addEstimateGlobalWaveletScale(false);
    }


  if(parseLocalWavelet(root, errTxt)==false)
  {
    inputFiles_->addShiftFile("");
    inputFiles_->addScaleFile("");
    modelSettings_->addEstimateLocalShift(false);
    modelSettings_->addEstimateLocalScale(false);   
  }

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseLocalWavelet(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("local-wavelet");
  if(root == 0)
  {  
    return(false);
  }

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="shift-file";
  legalCommands[1]="scale-file";
  legalCommands[2]="estimate-shift";
  legalCommands[3]="estimate-scale";

  modelSettings_->setUseLocalWavelet(true);
  std::string filename;
  bool shiftGiven = parseFileName(root, "shift-file", filename, errTxt);
  if(shiftGiven)
    inputFiles_->addShiftFile(filename);
  else
    inputFiles_->addShiftFile("");

  bool scaleGiven = parseFileName(root, "scale-file", filename, errTxt);
  if(scaleGiven)
    inputFiles_->addScaleFile(filename);
  else
    inputFiles_->addScaleFile("");

  bool estimate;
  std::string tmpErr;
  if(parseBool(root, "estimate-shift",estimate,tmpErr) == false || tmpErr != "") {
    modelSettings_->addEstimateLocalShift(false);
    errTxt += tmpErr;
  }
  else 
    if(estimate == true && shiftGiven == true) {
      errTxt += "Can not give both file and ask for estimation of local wavelet shift"+
        lineColumnText(node);
      modelSettings_->addEstimateLocalShift(false);
    }
    else
      modelSettings_->addEstimateLocalShift(estimate);

  tmpErr = "";
  if(parseBool(root, "estimate-scale",estimate,tmpErr) == false || tmpErr != "") {
    modelSettings_->addEstimateLocalScale(false);
    errTxt += tmpErr;
  }
  else 
    if(estimate == true && scaleGiven == true) {
      errTxt += "Can not give both file and ask for estimation of local wavelet scale"+
        lineColumnText(node);
      modelSettings_->addEstimateLocalScale(false);
    }
    else
      modelSettings_->addEstimateLocalScale(estimate);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWaveletEstimationInterval(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("wavelet-estimation-interval");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(2);
  legalCommands[0]="top-surface-file";
  legalCommands[1]="base-surface-file";

  std::string filename;
  if(parseFileName(root, "top-surface-file", filename, errTxt) == true)
    inputFiles_->setWaveletEstIntFile(0, filename);
  if(parseFileName(root, "base-surface-file", filename, errTxt) == true)
    inputFiles_->setWaveletEstIntFile(1, filename);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parsePriorModel(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("prior-model");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(6);
  legalCommands[0]="background";
  legalCommands[1]="lateral-correlation";
  legalCommands[2]="temporal-correlation";
  legalCommands[3]="parameter-correlation";
  legalCommands[4]="correlation-direction";
  legalCommands[5]="facies-probabilities";

  parseBackground(root, errTxt);

  Vario* vario = NULL;
  if(parseVariogram(root, "lateral-correlation", vario, errTxt) == true) {
    if (vario != NULL) {
      modelSettings_->setLateralCorr(vario);
    }
  }

  std::string filename;
  if(parseFileName(root, "temporal-correlation", filename, errTxt) == true)
    inputFiles_->setTempCorrFile(filename);

  if(parseFileName(root, "parameter-correlation", filename, errTxt) == true)
    inputFiles_->setParamCorrFile(filename);

  if(parseFileName(root, "correlation-direction", filename, errTxt) == true)
    inputFiles_->setCorrDirFile(filename);

  parseFaciesProbabilities(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseBackground(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("background");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(10);
  legalCommands[0]="direct-files";
  legalCommands[1]="vp-file";
  legalCommands[2]="vs-file";
  legalCommands[3]="density-file";
  legalCommands[4]="vp-constant";
  legalCommands[5]="vs-constant";
  legalCommands[6]="density-constant";
  legalCommands[7]="velocity-field";
  legalCommands[8]="lateral-correlation";
  legalCommands[9]="high-cut-background-modelling";

  bool direct = false;
  parseBool(root, "direct-files", direct, errTxt);
  modelSettings_->setDirectBGInput(direct);

  std::string filename;
  bool vp = parseFileName(root, "vp-file", filename, errTxt);
  if(vp == true) {
    inputFiles_->setBackFile(0, filename);
    modelSettings_->setConstBackValue(0, -1);
  }

  bool vs = parseFileName(root, "vs-file", filename, errTxt);
  if(vp == true) {
    inputFiles_->setBackFile(1, filename);
    modelSettings_->setConstBackValue(1, -1);
  }

  bool rho = parseFileName(root, "density-file", filename, errTxt);
  if(vp == true) {
    inputFiles_->setBackFile(2, filename);
    modelSettings_->setConstBackValue(2, -1);
  }

  float value;
  if(parseValue(root, "vp-constant", value, errTxt) == true) {
    if(vp == true)
      errTxt += "Both file and constant given for vp in command <"+root->ValueStr()+"> "+
        lineColumnText(root)+".\n";
    else {
      vp = true;
      modelSettings_->setConstBackValue(0, value);
    }
  }

  if(parseValue(root, "vs-constant", value, errTxt) == true) {
    if(vs == true)
      errTxt += "Both file and constant given for vs in command <"+root->ValueStr()+"> "+
        lineColumnText(root)+".\n";
    else {
      vs = true;
      modelSettings_->setConstBackValue(1, value);
    }
  }

  if(parseValue(root, "density-constant", value, errTxt) == true) {
    if(rho == true)
      errTxt += "Both file and constant given for density in command <"+root->ValueStr()+"> "+
        lineColumnText(root)+".\n";
    else {
      rho = true;
      modelSettings_->setConstBackValue(2, value);
    }
  }

  bool bgGiven = (vp & vs & rho);
  bool estimate = !(vp | vs | rho);
  modelSettings_->setGenerateBackground(estimate);
  if((bgGiven | estimate) == false) {
    errTxt +=  "Either all or no background parameters must be given in command <"+root->ValueStr()+"> "+
        lineColumnText(root)+".\n";
  }

  if(parseFileName(root, "velocity-field", filename, errTxt) == true)
    inputFiles_->setBackVelFile(filename);
  
  Vario * vario = NULL;
  if(parseVariogram(root, "lateral-correlation", vario, errTxt) == true) {
    if (vario != NULL) {
      modelSettings_->setBackgroundVario(vario);
    }
  }

  if(parseValue(root, "high-cut-background-modelling", value, errTxt) == true)
    modelSettings_->setMaxHzBackground(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseFaciesProbabilities(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("facies-probabilities");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="facies-estimation-interval";
  legalCommands[1]="prior-facies-probabilities";
  legalCommands[2]="facies-probability-undefined-value";

  parseFaciesEstimationInterval(root, errTxt);

  parsePriorFaciesProbabilities(root, errTxt);

  float value;
  if(parseValue(root, "facies-probability-undefined-value", value, errTxt) == true)
    modelSettings_->setPundef(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseFaciesEstimationInterval(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("facies-estimation-interval");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(2);
  legalCommands[0]="top-file-name";
  legalCommands[1]="base-file-name";

  std::string filename;
  if(parseFileName(root, "top-file-name", filename, errTxt) == true)
    inputFiles_->setFaciesEstIntFile(0, filename);
  else
    errTxt += "Must specify <top-file-name> in command <"+root->ValueStr()+"> "+
      lineColumnText(root)+".\n";
  if(parseFileName(root, "base-file-name", filename, errTxt) == true)
    inputFiles_->setFaciesEstIntFile(1, filename);
  else
    errTxt += "Must specify <base-file-name> in command <"+root->ValueStr()+">"+
      lineColumnText(root)+".\n";

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}

bool
XmlModelFile::parsePriorFaciesProbabilities(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("prior-facies-probabilities");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="facies";
  legalCommands[1]="prob";
  legalCommands[2]="probcube";
  
  float sum;
  int status = 0;
  sum = 0.0;
  int oldStatus = 0;
 
 // std::map<std::string,float> pp;
 // std::map<std::string, std::string> pp2;
  modelSettings_->setPriorFaciesProbGiven(0);
  while(parseFacies(root,errTxt)==true)
  {
    status = modelSettings_->getIsPriorFaciesProbGiven();
    if(oldStatus!=0 &&oldStatus!=status)
      errTxt+= "Prior facies probability must be given in the same way for all facies.\n";
   
    oldStatus = status;
   
  }
  if(status==1)
  {
    typedef std::map<std::string,float> mapType;
    mapType myMap = modelSettings_->getPriorFaciesProb();
    for(mapType::const_iterator it=myMap.begin();it!=myMap.end();++it)
    {
      sum+=(*it).second;
    }
  if(sum!=1.0)
  {
    errTxt+="Prior facies probabilities must sum to 1.0. They sum to "+ NRLib::ToString(sum) +".\n";
  }
 
  }
checkForJunk(root, errTxt, legalCommands);
  return(true);
}

bool 
XmlModelFile::parseFacies(TiXmlNode * node, std::string & errTxt)
{
TiXmlNode * root = node->FirstChildElement("facies");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="name";
  legalCommands[1]="probability";
  legalCommands[2]="probabilitycube";

  std::string faciesname;
  std::string filename;
  float value;
  parseValue(root, "name", faciesname,errTxt, true);
  
    if(parseValue(root,"probability",value,errTxt,true)==true)
    {
     modelSettings_->setPriorFaciesProbGiven(1);
     modelSettings_->addPriorFaciesProb(faciesname,value);
     // status = 1;
    }
    else if(parseValue(root,"probabilitycube",filename,errTxt,true)==true)
    {
     modelSettings_->setPriorFaciesProbGiven(2);
     inputFiles_->setPriorFaciesProb(faciesname,filename);
     // status = 2;
    }
 
 checkForJunk(root, errTxt, legalCommands, true); //allow duplicates
  return(true);


}
bool
XmlModelFile::parseProjectSettings(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("project-settings");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(3);
  legalCommands[0] = "output-volume";
  legalCommands[1] = "io-settings";
  legalCommands[2] = "advanced-settings";

  if(parseOutputVolume(root, errTxt) == false)
    errTxt += "Command <output-volume> is needed in command <"+
      root->ValueStr()+">"+lineColumnText(root)+".\n";

  parseIOSettings(root, errTxt);
  parseAdvancedSettings(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseOutputVolume(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("output-volume");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0] = "interval-two-surfaces";
  legalCommands[1] = "interval-one-surface";
  legalCommands[2] = "area";

  bool interval = parseIntervalTwoSurfaces(root, errTxt);
  
  if(parseIntervalOneSurface(root, errTxt) == interval) { //Either both or none given
    if(interval == true)
      errTxt += "Time interval specified in more than one way in command <"+root->ValueStr()+"> "
        +lineColumnText(root)+".\n";
    else
      errTxt += "No time interval specified in command <"+root->ValueStr()+"> "
        +lineColumnText(root)+".\n";
  }

  parseArea(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseIntervalTwoSurfaces(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("interval-two-surfaces");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(6);
  legalCommands[0] = "top-surface";
  legalCommands[1] = "base-surface";
  legalCommands[2] = "number-of-layers";
  legalCommands[3] = "direct-file";
  legalCommands[4] = "velocity-field";
  legalCommands[5] = "velocity-field-from-inversion";

  modelSettings_->setParallelTimeSurfaces(false);

  if(parseTopSurface(root, errTxt) == false)
    errTxt += "Top surface not specified in command <"+root->ValueStr()+"> "
      +lineColumnText(root)+".\n";
  bool topDepthGiven;
  if(inputFiles_->getDepthSurfFile(0) == "")
    topDepthGiven = false;
  else
    topDepthGiven = true;

  if(parseBaseSurface(root, errTxt) == false)
    errTxt += "Base surface not specified in command <"+root->ValueStr()+"> "
      +lineColumnText(root)+".\n";
  bool baseDepthGiven;
  if(inputFiles_->getDepthSurfFile(1) == "")
    baseDepthGiven = false;
  else
    baseDepthGiven = true;

  int value = 0;
  if(parseValue(root, "number-of-layers", value, errTxt) == false)
    errTxt += "Number of layers not specified in command <"+root->ValueStr()+">"
      +lineColumnText(root)+".\n";
  else
    modelSettings_->setTimeNz(value);

  bool directVel = false;
  parseBool(root,"direct-file", directVel, errTxt);
  modelSettings_->setDirectVelInput(directVel);

  std::string filename;
  bool externalField = parseFileName(root, "velocity-field", filename, errTxt);
  if(externalField == true)
    inputFiles_->setVelocityField(filename);

  bool inversionField = false;
  parseBool(root, "velocity-field-from-inversion", inversionField, errTxt);
  

  if(inversionField == true) {
    modelSettings_->setVelocityFromInversion(true);
    if(externalField == true)
      errTxt += "Both <velcoity-field> and <velocity-field-from-inversion> given in command <"
        +root->ValueStr()+">"+lineColumnText(root)+".\n";
  }

  if(externalField == false && inversionField == false) {
    if(topDepthGiven != baseDepthGiven)
      errTxt += "Only one depth surface given, and no velocity field in command <"
        +root->ValueStr()+"> "+lineColumnText(root)+".\n";
    else if(topDepthGiven == true) // Both top and bottom given.
      modelSettings_->setDepthDataOk(true);
  }
  else {
    if(topDepthGiven == false && baseDepthGiven == false) {
      errTxt += "Velocity field, but no depth surface given in command <"
        +root->ValueStr()+"> "+lineColumnText(root)+".\n";
    }
    else
      modelSettings_->setDepthDataOk(true); //One or two surfaces, and velocity field.
  }

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseTopSurface(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("top-surface");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0] = "time-file";
  legalCommands[1] = "time-value";
  legalCommands[2] = "depth-file";

  std::string filename;
  bool timeFile = parseFileName(root,"time-file", filename, errTxt);
  if(timeFile == true)
    inputFiles_->addTimeSurfFile(filename);

  float value;
  bool timeValue = parseValue(root,"time-value", value, errTxt);
  if(timeValue == true) {
    if(timeFile == false)
      inputFiles_->addTimeSurfFile(NRLib::ToString(value));
    else
      errTxt += "Both file and value given for top time in command <"
        +root->ValueStr()+"> "+lineColumnText(root)+".\n";
  }
  else if(timeFile == false) {
    inputFiles_->addTimeSurfFile("");

    errTxt += "No time surface given in command <"+root->ValueStr()+"> "
      +lineColumnText(root)+".\n";
  }

  if(parseFileName(root,"depth-file", filename, errTxt) == true)
    inputFiles_->setDepthSurfFile(0, filename);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseBaseSurface(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("base-surface");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(3);
  legalCommands[0] = "time-file";
  legalCommands[1] = "time-value";
  legalCommands[2] = "depth-file";

  std::string filename;
  bool timeFile = parseFileName(root,"time-file", filename, errTxt);
  if(timeFile == true)
    inputFiles_->addTimeSurfFile(filename);

  float value;
  bool timeValue = parseValue(root,"time-value", value, errTxt);
  if(timeValue == true) {
    if(timeFile == false)
      inputFiles_->addTimeSurfFile(NRLib::ToString(value));
    else
      errTxt += "Both file and value given for base time in command <"+
        root->ValueStr()+"> "+lineColumnText(root)+".\n";
  }
  else if(timeFile == false) {
    inputFiles_->addTimeSurfFile("");
    errTxt += "No time surface given in command <"+root->ValueStr()+"> "
      +lineColumnText(root)+".\n";
  }

  if(parseFileName(root,"depth-file", filename, errTxt) == true)
    inputFiles_->setDepthSurfFile(1, filename);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseIntervalOneSurface(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("interval-one-surface");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0] = "reference-surface";
  legalCommands[1] = "shift-to-interval-top";
  legalCommands[2] = "thickness";
  legalCommands[3] = "sample-density";

  modelSettings_->setParallelTimeSurfaces(true);

  std::string filename;
  if(parseFileName(root, "reference-surface", filename, errTxt) == true)
    inputFiles_->addTimeSurfFile(filename);
  else
    errTxt += "No reference surface given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double value;
  if(parseValue(root, "shift-to-interval-top", value, errTxt) == true)
    modelSettings_->setTimeDTop(value);

  if(parseValue(root, "thickness", value, errTxt) == true)
    modelSettings_->setTimeLz(value);
  else
    errTxt += "No thickness given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  if(parseValue(root, "sample-density", value, errTxt) == true)
    modelSettings_->setTimeDz(value);
  else
    errTxt += "No sample density given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseArea(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("area");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(7);
  legalCommands[0] = "reference-point-x";
  legalCommands[1] = "reference-point-y";
  legalCommands[2] = "length-x";
  legalCommands[3] = "length-y";
  legalCommands[4] = "sample-density-x";
  legalCommands[5] = "sample-density-y";
  legalCommands[6] = "angle";
  
  double x0 = 0;
  if(parseValue(root, "reference-point-x", x0, errTxt) == false)
    errTxt += "Reference x-coordinat must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double y0 = 0;
  if(parseValue(root, "reference-point-y", y0, errTxt) == false)
    errTxt += "Reference y-coordinat must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double lx = 0;
  if(parseValue(root, "length-x", lx, errTxt) == false)
    errTxt += "X-length must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double ly = 0;
  if(parseValue(root, "length-y", ly, errTxt) == false)
    errTxt += "Y-length must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double dx = 0;
  if(parseValue(root, "sample-density-x", dx, errTxt) == false)
    errTxt += "Sample density for x must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double dy = 0;
  if(parseValue(root, "sample-density-y", dy, errTxt) == false)
    errTxt += "Sample density for y must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double angle = 0;
  if(parseValue(root, "angle", angle, errTxt) == false)
    errTxt += "Rotation angle must be given in command <"+
      root->ValueStr()+"> "+lineColumnText(root)+".\n";

  double rot = (-1)*angle*(M_PI/180.0);
  int nx = static_cast<int>(lx/dx);
  int ny = static_cast<int>(ly/dy);
  SegyGeometry * geometry = new SegyGeometry(x0, y0, dx, dy, nx, ny, rot);
  modelSettings_->setAreaParameters(geometry);
  delete geometry;

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseIOSettings(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("io-settings");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(6);
  legalCommands[0]="top-directory";
  legalCommands[1]="input-directory";
  legalCommands[2]="output-directory";
  legalCommands[3]="output-types";
  legalCommands[4]="file-output-prefix";
  legalCommands[5]="log-level";

  std::string topDir;
  parseValue(root, "top-directory", topDir, errTxt);
  ensureTrailingSlash(topDir);

  std::string inputDir;
  parseValue(root, "input-directory", inputDir, errTxt);
  inputDir = topDir+inputDir;
  ensureTrailingSlash(inputDir);
  inputFiles_->setInputDirectory(inputDir);
  
  std::string outputDir;
  parseValue(root, "output-directory", outputDir, errTxt);
  outputDir = topDir+outputDir;
  ensureTrailingSlash(outputDir);
  ModelSettings::setOutputPath(outputDir);

  parseOutputTypes(root, errTxt);

  std::string value;
  if(parseValue(root, "file-output-prefix", value, errTxt) == true)
    ModelSettings::setFilePrefix(value);

  std::string level;
  if(parseValue(root, "log-level", level, errTxt) == true) {
    int logLevel = LogKit::ERROR;
    if(level=="error")
      logLevel = LogKit::L_ERROR;
    else if(level=="warning")
      logLevel = LogKit::L_WARNING;
    else if(level=="low")
      logLevel = LogKit::L_LOW;
    else if(level=="medium")
      logLevel = LogKit::L_MEDIUM;
    else if(level=="high")
      logLevel = LogKit::L_HIGH;
    else if(level=="debuglow")
      logLevel = LogKit::L_DEBUGLOW;
    else if(level=="debughigh")
      logLevel = LogKit::L_DEBUGHIGH;
    else {
      errTxt += "Unknown log level " + level + " in command <log-level>. ";
      errTxt += "Choose from: error, warning, low, medium, and high\n";
      return(false);
    }
    modelSettings_->setLogLevel(logLevel);
  }

  //Test-open log file, to check valid path.
  std::string logFileName = ModelSettings::makeFullFileName("logFile.txt");
  std::ofstream file;
  try {
    NRLib::OpenWrite(file, logFileName);
  }
  catch(NRLib::Exception & e) {
    errTxt +=  "Cannot open file '" + logFileName +"' : " + e.what()+"\n";
  }
  file.close();

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


void
XmlModelFile::ensureTrailingSlash(std::string & directory)
{
  std::string slash("/");

  if (directory.find_last_of(slash) != directory.length() - 1) // There are no slashes present
    directory += slash;
}


bool
XmlModelFile::parseOutputTypes(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("output-types");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="grid-output";
  legalCommands[1]="well-output";
  legalCommands[2]="direct-output";
  legalCommands[3]="other-output";

  parseGridOutput(root, errTxt);
  parseWellOutput(root, errTxt);
  parseDirectOutput(root, errTxt);
  parseOtherOutput(root, errTxt);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseGridOutput(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("grid-output");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="domain";
  legalCommands[1]="format";
  legalCommands[2]="parameters";
  
  parseGridFormats(root, errTxt);
  parseGridDomains(root, errTxt);
  parseGridParameters(root, errTxt);

  //Set output for all FFTGrids.
  FFTGrid::setOutputFlags(modelSettings_->getOutputFormatFlag(), 
                          modelSettings_->getOutputDomainFlag());

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseGridDomains(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("domain");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(2);
  legalCommands[0]="depth";
  legalCommands[1]="time";

  bool useDomain = false;
  int domainFlag = modelSettings_->getOutputDomainFlag();
  if(parseBool(root, "time", useDomain, errTxt) == true) {
    if(useDomain == true)
      domainFlag = (domainFlag | ModelSettings::TIMEDOMAIN);
    else if((domainFlag & ModelSettings::TIMEDOMAIN) > 0)
      domainFlag -= ModelSettings::TIMEDOMAIN;
  }
  if(parseBool(root, "depth", useDomain, errTxt) == true) {
    if(useDomain == true) {
      domainFlag = (domainFlag | ModelSettings::DEPTHDOMAIN);
    }
    else if((domainFlag & ModelSettings::DEPTHDOMAIN) > 0)
      domainFlag -= ModelSettings::DEPTHDOMAIN;
  }
  modelSettings_->setOutputDomainFlag(domainFlag);

  if(domainFlag == 0)
    errTxt += "Both time and depth domain output turned off after command <"
      +root->ValueStr()+"> "+lineColumnText(root)+".\n";

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseGridFormats(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("format");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="segy";
  legalCommands[1]="storm";
  legalCommands[2]="ascii";
  legalCommands[3]="sgri";


  bool useFormat = false;
  int formatFlag = 0;
  bool stormSpecified = false;  //Default format, error if turned off and no others turned on.
  if(parseBool(root, "segy", useFormat, errTxt) == true && useFormat == true)
    formatFlag += ModelSettings::SEGY;
  if(parseBool(root, "storm", useFormat, errTxt) == true) {
    stormSpecified = true;
    if(useFormat == true)
      formatFlag += ModelSettings::STORM;
  }
  if(parseBool(root, "ascii", useFormat, errTxt) == true && useFormat == true)
    formatFlag += ModelSettings::ASCII;
  if(parseBool(root, "sgri", useFormat, errTxt) == true && useFormat == true)
    formatFlag += ModelSettings::SGRI;

  if(formatFlag > 0 || stormSpecified == true)
    modelSettings_->setOutputFormatFlag(formatFlag);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseGridParameters(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("parameters");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(16);
  legalCommands[0]="vp";
  legalCommands[1]="vs";
  legalCommands[2]="density";
  legalCommands[3]="lame-lambda";
  legalCommands[4]="lame-mu";
  legalCommands[5]="poisson-ratio";
  legalCommands[6]="ai";
  legalCommands[7]="si";
  legalCommands[8]="vp-vs-ratio";
  legalCommands[9]="murho";
  legalCommands[10]="lambdarho";
  legalCommands[11]="correlations";
  legalCommands[12]="residuals";
  legalCommands[13]="background";
  legalCommands[14]="background-trend";
  legalCommands[15]="extra-grids";

  bool value = false;
  int paramFlag = 0;
  if(modelSettings_->getDefaultGridOutputInd() == false) //May have set faciesprobs.
    paramFlag = modelSettings_->getGridOutputFlag();

  if(parseBool(root, "vp", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::VP;
  if(parseBool(root, "vs", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::VS;
  if(parseBool(root, "density", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::RHO;
  if(parseBool(root, "lame-lambda", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::LAMELAMBDA;
  if(parseBool(root, "lame-mu", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::LAMEMU;
  if(parseBool(root, "poisson-ratio", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::POISSONRATIO;
  if(parseBool(root, "ai", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::AI;
  if(parseBool(root, "si", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::SI;
  if(parseBool(root, "vp-vs-ratio", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::VPVSRATIO;
  if(parseBool(root, "murho", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::MURHO;
  if(parseBool(root, "lambdarho", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::LAMBDARHO;
  if(parseBool(root, "correlations", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::CORRELATION;
  if(parseBool(root, "residuals", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::RESIDUAL;
  if(parseBool(root, "background", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::BACKGROUND;
  if(parseBool(root, "background-trend", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::BACKGROUND_TREND;
  if(parseBool(root, "extra-grids", value, errTxt) == true && value == true)
    paramFlag += ModelSettings::EXTRA_GRIDS;

  if(paramFlag > 0) {
    modelSettings_->setDefaultGridOutputInd(false);
    modelSettings_->setGridOutputFlag(paramFlag);
  }

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWellOutput(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("well-output");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="format";
  legalCommands[1]="wells";
  legalCommands[2]="blocked-wells";
  legalCommands[3]="blocked-logs";

  parseWellFormats(root, errTxt);

  bool value;
  int wellFlag = 0;
  if(parseBool(root, "wells", value, errTxt) == true && value == true)
    wellFlag += ModelSettings::WELLS;
  if(parseBool(root, "blocked-wells", value, errTxt) == true && value == true)
    wellFlag += ModelSettings::BLOCKED_WELLS;
  if(parseBool(root, "blocked-logs", value, errTxt) == true && value == true)
    wellFlag += ModelSettings::BLOCKED_LOGS;

  modelSettings_->setWellOutputFlag(wellFlag);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseWellFormats(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("format");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(2);
  legalCommands[0]="rms";
  legalCommands[1]="norsar";

  bool useFormat = false;
  int formatFlag = 0;
  bool rmsSpecified = false;  //Default format, check if turned off.
  if(parseBool(root, "rms", useFormat, errTxt) == true) {
    rmsSpecified = true;
    if(useFormat == true)
      formatFlag += ModelSettings::RMSWELL;
  }
  if(parseBool(root, "norsar", useFormat, errTxt) == true && useFormat == true)
    formatFlag += ModelSettings::NORSARWELL;

  if(formatFlag > 0 || rmsSpecified == true)
    modelSettings_->setWellFormatFlag(formatFlag);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseDirectOutput(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("direct-output");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(3);
  legalCommands[0]="background";
  legalCommands[1]="seismic";
  legalCommands[2]="time-to-depth-velocity";

  bool value = false;
  parseBool(root, "background", value, errTxt);
  modelSettings_->setDirectBGOutput(value);

  value = false;
  parseBool(root, "seismic", value, errTxt);
  modelSettings_->setDirectSeisOutput(value);

  value = false;
  parseBool(root, "time-to-depth-velocity", value, errTxt);
  modelSettings_->setDirectVelOutput(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseOtherOutput(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("other-output");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(4);
  legalCommands[0]="wavelets";
  legalCommands[1]="extra-surfaces";
  legalCommands[2]="prior-correlations";
  legalCommands[3]="background-trend-1d";

  bool value;
  int otherFlag = 0;
  if(parseBool(root, "wavelets", value, errTxt) == true && value == true)
    otherFlag += ModelSettings::WAVELETS;
  if(parseBool(root, "extra-surfaces", value, errTxt) == true && value == true)
    otherFlag += ModelSettings::EXTRA_SURFACES;
  if(parseBool(root, "prior-correlations", value, errTxt) == true && value == true)
    otherFlag += ModelSettings::PRIORCORRELATIONS;
  if(parseBool(root, "background-trend-1d", value, errTxt) == true && value == true)
    otherFlag += ModelSettings::BACKGROUND_TREND_1D;

  modelSettings_->setOtherOutputFlag(otherFlag);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseAdvancedSettings(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("advanced-settings");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(12);
  legalCommands[0]="fft-grid-padding";
  legalCommands[1]="use-intermediate-disk-storage";
  legalCommands[2]="maximum-relative-thickness-difference";
  legalCommands[3]="frequency-band";
  legalCommands[4]="energy-threshold";
  legalCommands[5]="wavelet-tapering-length";
  legalCommands[6]="minimum-relative-wavelet-amplitude";
  legalCommands[7]="maximum-wavelet-shift";
  legalCommands[8]="white-noise-component";
  legalCommands[9]="reflection-matrix";
  legalCommands[10]="kriging-data-limit";
  legalCommands[11]="debug-level";

  parseFFTGridPadding(root, errTxt);

  int fileGrid;
  if(parseValue(root, "use-intermediate-disk-storage", fileGrid, errTxt) == true)
    modelSettings_->setFileGrid(fileGrid);
  double limit;
  if(parseValue(root,"maximum-relative-thickness-difference", limit, errTxt) == true)
    modelSettings_->setLzLimit(limit);
  
  parseFrequencyBand(root, errTxt);

  float value = 0.0f;
  if(parseValue(root, "energy-threshold", value, errTxt) == true)
    modelSettings_->setEnergyThreshold(value);
  if(parseValue(root, "wavelet-tapering-length", value, errTxt) == true)
    modelSettings_->setWaveletTaperingL(value);
  if(parseValue(root, "minimum-relative-wavelet-amplitude", value, errTxt) == true)
    modelSettings_->setMinRelWaveletAmp(value);
  if(parseValue(root, "maximum-wavelet-shift", value, errTxt) == true)
    modelSettings_->setMaxWaveletShift(value);
  if(parseValue(root, "white-noise-component", value, errTxt) == true)
    modelSettings_->setWNC(value);
  int level = 0;
  if(parseValue(root, "debug-level", level, errTxt) == true)
    modelSettings_->setDebugFlag(level);
  std::string filename;
  if(parseFileName(root, "reflection-matrix", filename, errTxt) == true)
    inputFiles_->setReflMatrFile(filename);
  int kLimit = 0;
  if(parseValue(root, "kriging-data-limit", kLimit, errTxt) == true) {
    if(modelSettings_->getKrigingParameter() >= 0)
      modelSettings_->setKrigingParameter(kLimit);
  }
  if(parseValue(root, "debug-level", level, errTxt) == true)
    modelSettings_->setDebugFlag(level);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseFFTGridPadding(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("fft-grid-padding");
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(3);
  legalCommands[0]="x-fraction";
  legalCommands[1]="y-fraction";
  legalCommands[2]="z-fraction";

  double padding;
  if(parseValue(root, "x-fraction", padding, errTxt) == true)
    modelSettings_->setXPadFac(padding);
  if(parseValue(root, "y-fraction", padding, errTxt) == true)
    modelSettings_->setYPadFac(padding);
  if(parseValue(root, "z-fraction", padding, errTxt) == true)
    modelSettings_->setZPadFac(padding);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseFrequencyBand(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement("frequency-band");
  if(root == 0)
    return(false);
  
  std::vector<std::string> legalCommands(2);
  legalCommands[0]="low-cut";
  legalCommands[1]="high-cut";

  float value;
  if(parseValue(root, "low-cut", value, errTxt) == true)
    modelSettings_->setLowCut(value);
  if(parseValue(root, "high-cut", value, errTxt) == true)
    modelSettings_->setHighCut(value);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


/*
bool
XmlModelFile::parse(TiXmlNode * node, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement();
  if(root == 0)
    return(false);

  checkForJunk(root, errTxt);
  return(true);
}
*/


bool
XmlModelFile::parseTraceHeaderFormat(TiXmlNode * node, const std::string & keyword, TraceHeaderFormat *& thf, std::string & errTxt)
{
  thf = NULL;
  TiXmlNode * root = node->FirstChildElement(keyword);
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(7);
  legalCommands[0]="standard-format";
  legalCommands[1]="location-x";
  legalCommands[2]="location-y";
  legalCommands[3]="location-il";
  legalCommands[4]="location-xl";
  legalCommands[5]="location-scaling-coefficient";
  legalCommands[6]="bypass-coordinate-scaling";

  std::string stdFormat;
  if(parseValue(root, "standard-format", stdFormat, errTxt) == true) {
    if(stdFormat == "seisworks")
      thf = new TraceHeaderFormat(TraceHeaderFormat::SEISWORKS);
    else if(stdFormat == "iesx")
      thf = new TraceHeaderFormat(TraceHeaderFormat::IESX);
    else {
      errTxt += "Unknown segy-format '"+stdFormat+"' found on line"+
        NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
      thf = new TraceHeaderFormat(TraceHeaderFormat::SEISWORKS);
    }
  }
  else
    thf = new TraceHeaderFormat(TraceHeaderFormat::SEISWORKS);

  int value;
  if(parseValue(root,"location-x",value, errTxt) == true)
    thf->SetUtmxLoc(value);
  if(parseValue(root,"location-y",value, errTxt) == true)
    thf->SetUtmyLoc(value);
  if(parseValue(root,"location-il",value, errTxt) == true)
    thf->SetInlineLoc(value);
  if(parseValue(root,"location-xl",value, errTxt) == true)
    thf->SetCrosslineLoc(value);
  if(parseValue(root,"location-scaling-coefficient",value, errTxt) == true)
    thf->SetScaleCoLoc(value);

  bool bypass;
  if(parseBool(root,"bypass-coordinate-scaling", bypass, errTxt) == true)
    if(bypass == true)
      thf->SetScaleCoLoc(-1);

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseVariogram(TiXmlNode * node, const std::string & keyword, Vario * & vario, std::string & errTxt)
{
  TiXmlNode * root = node->FirstChildElement(keyword);
  if(root == 0)
    return(false);

  std::vector<std::string> legalCommands(5);
  legalCommands[0]="range";
  legalCommands[1]="subrange";
  legalCommands[2]="angle";
  legalCommands[3]="power";
  legalCommands[4]="variogram-type";

  float value, range = 1;
  float subrange=1;
  float angle = 0;
  float expo = 1;
  if(parseValue(root,"range", value, errTxt) == true)
    range = value;
  else
    errTxt += "Keyword <range> is lacking for variogram under command <"+keyword+
      ">, line "+NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
  if(parseValue(root,"subrange", value, errTxt) == true)
    subrange = value;
  if(parseValue(root,"angle", value, errTxt) == true)
    angle = 90.0f-value; //From geological to mathematical.
  bool power = parseValue(root,"power", value, errTxt);
  if(power == true)
    expo = value;

  if (range==0.0) {
    errTxt += "The value of <range> given for command <"+keyword+"> must be greater than zero.\n";
  }
  if (subrange==0.0) {
    errTxt += "The value of <subrange> given for command <"+keyword+"> must be greater than zero.\n";
  }

  std::string vType;
  vario = NULL;
  if(parseValue(root,"variogram-type", vType, errTxt) == false)
    errTxt += "Keyword <variogram-type> is lacking for variogram under command <"+keyword+
      ">, line "+NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
  else {
    if(vType == "genexp") {
      if(power == false) {
        errTxt += "Keyword <power> is lacking for gen. exp. variogram under command <"+keyword+
          ">, line "+NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
      }
      vario = new GenExpVario(expo, range, subrange, angle);
    }
    else if(vType == "spherical") {
      if(power == true) {
        errTxt += "Keyword <power> is given for spherical variogram under command <"+keyword+
          ">, line "+NRLib::ToString(root->Row())+", column "+NRLib::ToString(root->Column())+".\n";
      }
      vario = new SphericalVario(range, subrange, angle);
    }
  }

  checkForJunk(root, errTxt, legalCommands);
  return(true);
}


bool
XmlModelFile::parseBool(TiXmlNode * node, const std::string & keyword, bool & value, std::string & errTxt, bool allowDuplicates)
{
  std::string tmpVal;
  std::string tmpErr = "";
  if(parseValue(node, keyword, tmpVal, tmpErr, allowDuplicates) == false)
    return(false);

  //Keyword is found.
  if(tmpErr == "") {
    if(tmpVal == "yes")
      value = true;
    else if(tmpVal == "no")
      value = false;
    else {
      tmpErr = "Found '"+tmpVal+"' under keyword '"+keyword+"', expected 'yes' or 'no'. This happened in command <"+
        node->ValueStr()+"> on line "+NRLib::ToString(node->Row())+", column "+NRLib::ToString(node->Column())+".\n";
    }
  }

  //No junk-clearing call, done in parseValue.
  errTxt += tmpErr;
  return(true);
}


bool
XmlModelFile::parseFileName(TiXmlNode * node, const std::string & keyword, std::string & filename, std::string & errTxt, bool allowDuplicates)
{
  filename = "";
  std::string value;
  std::string tmpErr;
  if(parseValue(node, keyword, value, tmpErr, allowDuplicates) == false)
    return(false);
  
  filename = value;

  //No junk-clearing, done in parseValue
  errTxt += tmpErr;
  return(true);
}



void 
XmlModelFile::checkForJunk(TiXmlNode * root, std::string & errTxt, const std::vector<std::string> & legalCommands, 
                    bool allowDuplicates)
{
  TiXmlNode * child = root->FirstChild();
  unsigned int startLength = errTxt.size();
  while(child != NULL) {
    switch(child->Type()) {
      case TiXmlNode::COMMENT :
      case TiXmlNode::DECLARATION :
        break;
      case TiXmlNode::TEXT :
        errTxt = errTxt + "Unexpected value '"+child->Value()+"' is not part of command <"+root->Value()+
          "> on line "+NRLib::ToString(child->Row())+", column "+NRLib::ToString(child->Column())+".\n";
        break;
      case TiXmlNode::ELEMENT :
        errTxt = errTxt + "Unexpected command <"+child->Value()+"> is not part of command <"+root->Value()+
          "> on line "+NRLib::ToString(child->Row())+", column "+NRLib::ToString(child->Column())+".\n";
        break;
      default :
        errTxt = errTxt + "Unexpected text '"+child->Value()+"' is not part of command <"+root->Value()+
          "> on line "+NRLib::ToString(child->Row())+", column "+NRLib::ToString(child->Column())+".\n";
        break;
    }
    root->RemoveChild(child);
    child = root->FirstChild();
  }

  if(startLength<errTxt.size()){
    errTxt = errTxt + "Legal commands are:\n";
    for(unsigned int i=0;i<legalCommands.size();i++)
      errTxt = errTxt +"<"+legalCommands[i]+">\n";
  }

  TiXmlNode * parent = root->Parent();

  if(parent != NULL) {
    std::string cmd = root->ValueStr();
    parent->RemoveChild(root);

    if(allowDuplicates == false) {
      root = parent->FirstChildElement(cmd);
      if(root != NULL) {
        int n = 0;
        while(root != NULL) {
          n++;
          parent->RemoveChild(root);
          root = parent->FirstChildElement(cmd);
        }
        errTxt += "Found "+NRLib::ToString(n)+" extra occurences of command <"+cmd+"> under command <"+parent->Value()+
          "> on line "+NRLib::ToString(parent->Row())+", column "+NRLib::ToString(parent->Column())+".\n";
      }
    }
  }
}


std::string
XmlModelFile::lineColumnText(TiXmlNode * node)
{
  std::string result = " on line "+NRLib::ToString(node->Row())+", column "+NRLib::ToString(node->Column());
  return(result);
}


void
XmlModelFile::checkConsistency(std::string & errTxt) {
  errTxt += inputFiles_->addInputPathAndCheckFiles();
  if(modelSettings_->getGenerateSeismic() == true)
    checkForwardConsistency(errTxt);
  else
    checkEstimationInversionConsistency(errTxt);
  if(modelSettings_->getLocalWaveletVario()==NULL) 
      modelSettings_->copyBackgroundVarioToLocalWaveletVario();
}


void
XmlModelFile::checkForwardConsistency(std::string & errTxt) {
  //Mostly, we don't care here, but have to straighten some things.
  if(modelSettings_->getGenerateSeismic() == true) {
    //Set dummy values
    int i;
    for(i=0;i<modelSettings_->getNumberOfAngles();i++)
      modelSettings_->setSNRatio(i,1.1f);
  }
}


void
XmlModelFile::checkEstimationInversionConsistency(std::string & errTxt) {
  //Quite a few things to check here.
}
