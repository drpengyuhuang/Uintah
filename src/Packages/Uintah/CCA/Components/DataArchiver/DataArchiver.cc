#include <Packages/Uintah/CCA/Components/DataArchiver/DataArchiver.h>
#include <Packages/Uintah/Core/Grid/Box.h>
#include <Packages/Uintah/Core/Grid/Grid.h>
#include <Packages/Uintah/Core/Grid/Level.h>
#include <Packages/Uintah/Core/Grid/Patch.h>
#include <Packages/Uintah/Core/Grid/Task.h>
#include <Packages/Uintah/Core/Grid/VarTypes.h>
#include <Packages/Uintah/CCA/Ports/DataWarehouse.h>
#include <Packages/Uintah/CCA/Ports/OutputContext.h>
#include <Packages/Uintah/Core/ProblemSpec/ProblemSpec.h>
#include <Packages/Uintah/CCA/Ports/Scheduler.h>
#include <Packages/Uintah/Core/Parallel/Parallel.h>
#include <Packages/Uintah/Core/Parallel/ProcessorGroup.h>
#include <Packages/Uintah/Core/Exceptions/ProblemSetupException.h>

#include <Dataflow/XMLUtil/SimpleErrorHandler.h>
#include <Dataflow/XMLUtil/XMLUtil.h>

#include <Core/Util/DebugStream.h>
#include <Core/Exceptions/ErrnoException.h>
#include <Core/Exceptions/InternalError.h>
#include <Core/Util/FancyAssert.h>
#include <Core/Util/NotFinished.h>
#include <Core/Util/Endian.h>
#include <Core/Thread/Time.h>

#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <iomanip>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>

#define PADSIZE 1024L

using namespace Uintah;
using namespace std;
using namespace SCIRun;

static Dir makeVersionedDir(const std::string nameBase);
static DebugStream dbg("DataArchiver", false);

DataArchiver::DataArchiver(const ProcessorGroup* myworld)
  : UintahParallelComponent(myworld),
    d_outputLock("DataArchiver output lock")
{
  d_wasOutputTimestep = false;
  d_wasCheckpointTimestep = false;
  d_saveParticleVariables = false;
  d_saveP_x = false;
  d_currentTime=-1;
  d_currentTimestep=-1;
}

DataArchiver::~DataArchiver()
{
}

void DataArchiver::problemSetup(const ProblemSpecP& params)
{
   ProblemSpecP p = params->findBlock("DataArchiver");
   p->require("filebase", d_filebase);
   d_outputInterval = 0;
   if(!p->get("outputTimestepInterval", d_outputTimestepInterval))
     d_outputTimestepInterval = 0;
   if(!p->get("outputInterval", d_outputInterval)
      && d_outputTimestepInterval == 0)
     d_outputInterval = 1.0; // default

   if (d_outputInterval != 0.0 && d_outputTimestepInterval != 0)
     throw ProblemSetupException("Use <outputInterval> or <outputTimestepInterval>, not both");
   
   string defaultCompressionMode = "";
   if (p->get("compression", defaultCompressionMode)) {
     VarLabel::setDefaultCompressionMode(defaultCompressionMode);
   }
   
   map<string, string> attributes;
   SaveNameItem saveItem;
   ProblemSpecP save = p->findBlock("save");
   while( save != 0 ) {
      attributes.clear();
      save->getAttributes(attributes);
      saveItem.labelName = attributes["label"];
      saveItem.compressionMode = attributes["compression"];
      try {
         saveItem.matls = ConsecutiveRangeSet(attributes["material"]);
      }
      catch (ConsecutiveRangeSetException) {
	throw ProblemSetupException("'" + attributes["material"] + "'" +
	       " cannot be parsed as a set of material" +
	       " indices for saving '" + saveItem.labelName + "'");
      }

      if (saveItem.matls.size() == 0)
	// if materials aren't specified, all valid materials will be saved
	saveItem.matls = ConsecutiveRangeSet::all;
      
       //__________________________________
       //  bullet proofing: must save p.x 
       //  in addition to other particle variables "p.*"
       if (saveItem.labelName == "p.x") {
         d_saveP_x = true;
       }

       string::size_type pos = saveItem.labelName.find("p.");
       if ( pos != string::npos &&  saveItem.labelName != "p.x") {
         d_saveParticleVariables = true;
       }

      d_saveLabelNames.push_back(saveItem);
      save = save->findNextBlock("save");
   }
   if(d_saveP_x == false && d_saveParticleVariables == true) {
     throw ProblemSetupException(" You must save p.x when saving other particle variables");
   }     
   
   d_checkpointInterval = 0.0;
   d_checkpointTimestepInterval = 0;
   d_checkpointWalltimeStart = 0;
   d_checkpointWalltimeInterval = 0;
   d_checkpointCycle = 2; /* 2 is the smallest number that is safe
			     (always keeping an older copy for backup) */
   ProblemSpecP checkpoint = p->findBlock("checkpoint");
   if (checkpoint != 0) {
      attributes.clear();
      checkpoint->getAttributes(attributes);
      string attrib = attributes["interval"];
      if (attrib != "")
	d_checkpointInterval = atof(attrib.c_str());
      attrib = attributes["timestepInterval"];
      if (attrib != "")
	d_checkpointTimestepInterval = atoi(attrib.c_str());
      attrib = attributes["walltimeStart"];
      if (attrib != "")
	d_checkpointWalltimeStart = atoi(attrib.c_str());
      attrib = attributes["walltimeInterval"];
      if (attrib != "")
	d_checkpointWalltimeInterval = atoi(attrib.c_str());
      attrib = attributes["cycle"];
      if (attrib != "")
	d_checkpointCycle = atoi(attrib.c_str());
   }

   // can't use both checkpointInterval and checkpointTimestepInterval
   if (d_checkpointInterval != 0.0 && d_checkpointTimestepInterval != 0)
     throw ProblemSetupException("Use <checkpoint interval=...> or <checkpoint timestepInterval=...>, not both");

   // can't have a walltimeStart without a walltimeInterval
   if (d_checkpointWalltimeStart != 0 && d_checkpointWalltimeInterval == 0)
     throw ProblemSetupException("<checkpoint walltimeStart must have a corresponding walltimeInterval");

   // set walltimeStart to walltimeInterval if not specified
   if (d_checkpointWalltimeInterval != 0 && d_checkpointWalltimeStart == 0)
     d_checkpointWalltimeStart = d_checkpointWalltimeInterval;
   
   d_currentTimestep = 0;
   
   int startTimestep;
   if (p->get("startTimestep", startTimestep)) {
     d_currentTimestep = startTimestep - 1;
   }
  
   d_lastTimestepLocation = "invalid";
   d_wasOutputTimestep = false;

   if (d_outputInterval == 0.0 && d_outputTimestepInterval == 0 && d_checkpointInterval == 0.0 && d_checkpointTimestepInterval == 0 && d_checkpointWalltimeInterval == 0) 
	return;

   d_nextOutputTime=0.0;
   d_nextOutputTimestep=1;
   d_nextCheckpointTime=d_checkpointInterval; // no need to checkpoint t=0
   d_nextCheckpointTimestep=d_checkpointTimestepInterval+1;
   
   if (d_checkpointWalltimeInterval > 0)
     d_nextCheckpointWalltime=d_checkpointWalltimeStart + Time::currentSeconds();
   else 
     d_nextCheckpointWalltime=0;

   if(Parallel::usingMPI()){
     // See how many shared filesystems that we have
     double start=Time::currentSeconds();
     string basename;
     if(d_myworld->myrank() == 0){
       // Create a unique string, using hostname+pid
       char* base = strdup(d_filebase.c_str());
       char* p = base+strlen(base);
       for(;p>=base;p--){
	 if(*p == '/'){
	   *p=0;
	   break;
	 }
       }
       if(*p){
	 free(base);
	 base = strdup(".");
       }

       char hostname[MAXHOSTNAMELEN];
       if(gethostname(hostname, MAXHOSTNAMELEN) != 0)
	 strcpy(hostname, "unknown???");
       ostringstream ts;
       ts << base << "-" << hostname << "-" << getpid();

       string test_string = ts.str();
       const char* outbuf = test_string.c_str();
       int outlen = (int)strlen(outbuf);

       MPI_Bcast(&outlen, 1, MPI_INT, 0, d_myworld->getComm());
       MPI_Bcast(const_cast<char*>(outbuf), outlen, MPI_CHAR, 0,
		 d_myworld->getComm());
       basename = test_string;
     } else {
       int inlen;
       MPI_Bcast(&inlen, 1, MPI_INT, 0, d_myworld->getComm());
       char* inbuf = scinew char[inlen+1];
       MPI_Bcast(inbuf, inlen, MPI_CHAR, 0, d_myworld->getComm());
       inbuf[inlen]='\0';
       basename=inbuf;
       delete[] inbuf;       
     }
     // Create a file, of the name p0_hostname-p0_pid-processor_number
     ostringstream myname;
     myname << basename << "-" << d_myworld->myrank() << ".tmp";
     string fname = myname.str();

     // This will be an empty file, everything is encoded in the name anyway
     FILE* tmpout = fopen(fname.c_str(), "w");
     if(!tmpout)
       throw ErrnoException("fopen failed for " + fname, errno);
     fprintf(tmpout, "\n");
     if(fflush(tmpout) != 0)
       throw ErrnoException("fflush", errno);
     if(fsync(fileno(tmpout)) != 0)
       throw ErrnoException("fsync", errno);
     if(fclose(tmpout) != 0)
       throw ErrnoException("fclose", errno);
     MPI_Barrier(d_myworld->getComm());
     // See who else we can see
     d_writeMeta=true;
     int i;
     for(i=0;i<d_myworld->myrank();i++){
       ostringstream name;
       name << basename << "-" << i << ".tmp";
       struct stat st;
       int s=stat(name.str().c_str(), &st);
       if(s == 0 && S_ISREG(st.st_mode)){
	 // File exists, we do NOT need to emit metadata
	 d_writeMeta=false;
	 break;
       } else if(errno != ENOENT){
	 cerr << "Cannot stat file: " << name.str() << ", errno=" << errno << '\n';
	 throw ErrnoException("stat", errno);
       }
     }
     MPI_Barrier(d_myworld->getComm());
     if(d_writeMeta){
       d_dir = makeVersionedDir(d_filebase);
       string fname = myname.str();
       FILE* tmpout = fopen(fname.c_str(), "w");
       if(!tmpout)
	 throw ErrnoException("fopen", errno);
       string dirname = d_dir.getName();
       fprintf(tmpout, "%s\n", dirname.c_str());
       if(fflush(tmpout) != 0)
	 throw ErrnoException("fflush", errno);
       if(fdatasync(fileno(tmpout)) != 0)
	 throw ErrnoException("fdatasync", errno);
       if(fclose(tmpout) != 0)
	 throw ErrnoException("fclose", errno);
     }
     MPI_Barrier(d_myworld->getComm());
     if(!d_writeMeta){
       ostringstream name;
       name << basename << "-" << i << ".tmp";
       ifstream in(name.str().c_str());
       if(!in){
	 throw InternalError("File not found on second pass for filesystem discovery!");
       }
       string dirname;
       in >> dirname;
       d_dir=Dir(dirname);
     }
     int count=d_writeMeta?1:0;
     int nunique;
     // This is an AllReduce, not a reduce.  This is necessary to
     // ensure that all processors wait before they remove the tmp files
     MPI_Allreduce(&count, &nunique, 1, MPI_INT, MPI_SUM,
		   d_myworld->getComm());
     if(d_myworld->myrank() == 0){
       double dt=Time::currentSeconds()-start;
       cerr << "Discovered " << nunique << " unique filesystems in " << dt << " seconds\n";
     }
     // Remove the tmp files...
     int s = unlink(myname.str().c_str());
     if(s != 0){
       cerr << "Cannot unlink file: " << myname.str() << '\n';
       throw ErrnoException("unlink", errno);
     }
   } else {
      d_dir = makeVersionedDir(d_filebase);
      d_writeMeta = true;
   }

   if (d_writeMeta) {
      string inputname = d_dir.getName()+"/input.xml";
      ofstream out(inputname.c_str());
      
      out << params->getNode()->getOwnerDocument() << endl; 
      createIndexXML(d_dir);
   
      if (d_checkpointInterval != 0.0 || d_checkpointTimestepInterval != 0 ||
	  d_checkpointWalltimeInterval != 0) {
	 d_checkpointsDir = d_dir.createSubdir("checkpoints");
	 createIndexXML(d_checkpointsDir);
      }
   }
   else
      d_checkpointsDir = d_dir.getSubdir("checkpoints");
}

// to be called after problemSetup gets called
void DataArchiver::restartSetup(Dir& restartFromDir, int startTimestep,
				int timestep, double time, bool fromScratch,
				bool removeOldDir)
{
   if (d_writeMeta && !fromScratch) {
      // partial copy of dat files
      copyDatFiles(restartFromDir, d_dir, startTimestep,
		   timestep, removeOldDir);

      copySection(restartFromDir, d_dir, "restarts");
      copySection(restartFromDir, d_dir, "variables");
      copySection(restartFromDir, d_dir, "globals");

      // partial copy of index.xml and timestep directories and
      // similarly for checkpoints
      copyTimesteps(restartFromDir, d_dir, startTimestep, timestep,
		    removeOldDir);
      Dir checkpointsFromDir = restartFromDir.getSubdir("checkpoints");
      bool areCheckpoints = true;
      copyTimesteps(checkpointsFromDir, d_checkpointsDir, startTimestep,
		    timestep, removeOldDir, areCheckpoints);
      copySection(checkpointsFromDir, d_checkpointsDir, "variables");
      copySection(checkpointsFromDir, d_checkpointsDir, "globals");
      if (removeOldDir)
	 restartFromDir.forceRemove();
   }
   else if (d_writeMeta) {
     // just add <restart from = ".." timestep = ".."> tag.
     copySection(restartFromDir, d_dir, "restarts");
     string iname = d_dir.getName()+"/index.xml";
     DOMDocument* indexDoc = loadDocument(iname);
     if (timestep >= 0)
       addRestartStamp(indexDoc, restartFromDir, timestep);
     ofstream copiedIndex(iname.c_str());
     copiedIndex << indexDoc << endl;
     indexDoc->release();
   }
   
   // set time and timestep variables appropriately
   d_currentTimestep = timestep;
   
   if (d_outputInterval > 0)
      d_nextOutputTime = d_outputInterval * ceil(time / d_outputInterval);
   else if (d_outputTimestepInterval > 0)
      d_nextOutputTimestep = d_currentTimestep + d_outputTimestepInterval;
   
   if (d_checkpointInterval > 0)
      d_nextCheckpointTime = d_checkpointInterval *
	 ceil(time / d_checkpointInterval);
   else if (d_checkpointTimestepInterval > 0)
      d_nextCheckpointTimestep = d_currentTimestep + d_checkpointTimestepInterval;
   if (d_checkpointWalltimeInterval > 0)
     d_nextCheckpointWalltime = d_checkpointWalltimeInterval + Time::currentSeconds();
}

//////////
// Call this when doing a combine_patches run after calling
// problemSetup.  It will copy the data files over and make it ignore
// dumping reduction variables.
void DataArchiver::combinePatchSetup(Dir& fromDir)
{
   if (d_writeMeta) {
     // partial copy of dat files
     copyDatFiles(fromDir, d_dir, 0, -1, false);
     copySection(fromDir, d_dir, "globals");
   }
   
   string iname = fromDir.getName()+"/index.xml";
   DOMDocument* indexDoc = loadDocument(iname);

   const DOMNode* globals = findNode("globals", indexDoc->getDocumentElement());
   if (globals != 0) {
      DOMNode* variable = const_cast<DOMNode*>(findNode("variable", globals));
      while (variable != 0) {
	DOMNamedNodeMap* attributes = variable->getAttributes();

	const XMLCh* attrName = to_xml_ch_ptr("name");
	DOMNode* nameNode = attributes->getNamedItem(attrName);


	if (nameNode == 0)
	  throw InternalError("global variable name attribute not found");
	const char* var = to_char_ptr(nameNode->getNodeValue());

	string varname = var;

	// this isn't the most efficient, but i don't think it matters
	// to much for this initialization code
	list<SaveNameItem>::iterator it = d_saveLabelNames.begin();
	while (it != d_saveLabelNames.end()) {
	  if ((*it).labelName == varname) {
	    it = d_saveLabelNames.erase(it);
	  }
	  else {
	    it++;
	  }
	}
	variable = const_cast<DOMNode*>(findNextNode("variable", variable));
      }
   }

   // don't transfer checkpoints when combining patches
   d_checkpointInterval = 0.0;
   d_checkpointTimestepInterval = 0;
   d_checkpointWalltimeInterval = 0;

   // output every timestep -- each timestep is transferring data
   d_outputInterval = 0.0;
   d_outputTimestepInterval = 1;

   indexDoc->release();
}

void DataArchiver::copySection(Dir& fromDir, Dir& toDir, string section)
{
  string iname = fromDir.getName()+"/index.xml";
  DOMDocument* indexDoc = loadDocument(iname);

  iname = toDir.getName()+"/index.xml";
  DOMDocument* myIndexDoc = loadDocument(iname);
  
   DOMNode* sectionNode = const_cast<DOMNode*>(findNode(section, indexDoc->getDocumentElement()));
  if (sectionNode != 0) {
    DOMNode* newNode = myIndexDoc->importNode(sectionNode, true);
    
    // replace whatever was in the section previously
    DOMNode* mySectionNode = const_cast<DOMNode*>(findNode(section, myIndexDoc->getDocumentElement()));
    if (mySectionNode != 0) {
      myIndexDoc->getDocumentElement()->replaceChild(newNode, mySectionNode);
    }
    else {
      const XMLCh* newline = to_xml_ch_ptr("\n");
      myIndexDoc->getDocumentElement()->appendChild(myIndexDoc->createTextNode(newline));

      myIndexDoc->getDocumentElement()->appendChild(newNode);
    }
  }
  
  ofstream indexOut(iname.c_str());
  indexOut << myIndexDoc << endl;

  indexDoc->release();
  myIndexDoc->release();
}

void DataArchiver::addRestartStamp(DOMDocument* indexDoc, Dir& fromDir,
				   int timestep)
{
   const XMLCh* str = to_xml_ch_ptr("\n");
   DOMText* leader = indexDoc->createTextNode(str);
   
   str = to_xml_ch_ptr("\n\t");
   DOMText* returnTab = indexDoc->createTextNode(str);
   
   DOMNode* restarts = const_cast<DOMNode*>(findNode("restarts", indexDoc->getDocumentElement()));
   if (restarts == 0) {
     str = to_xml_ch_ptr("restarts");
     restarts = indexDoc->createElement(str);

     indexDoc->getDocumentElement()->appendChild(leader);
     indexDoc->getDocumentElement()->appendChild(restarts);
   }
   str = to_xml_ch_ptr("restart");
   DOMElement* restartInfo = indexDoc->createElement(str);
   
   // need to call to_xml_ch_ptr2 for attrvalue because there is only one 
   // buffer for to_xml_ch_ptr
   const XMLCh* attrName = to_xml_ch_ptr("from"); 
   const XMLCh* attrValue = to_xml_ch_ptr2(fromDir.getName().c_str());
   restartInfo->setAttribute(attrName, attrValue);
   
   ostringstream timestep_str;
   timestep_str << timestep;
   
   attrName = to_xml_ch_ptr("timestep"); 
   attrValue = to_xml_ch_ptr2(timestep_str.str().c_str());

   restartInfo->setAttribute(attrName, attrValue);

   restarts->appendChild(returnTab);
   restarts->appendChild(restartInfo);
   restarts->appendChild(leader);
}

void DataArchiver::copyTimesteps(Dir& fromDir, Dir& toDir, int startTimestep,
				 int maxTimestep, bool removeOld,
				 bool areCheckpoints /*=false*/)
{
   string old_iname = fromDir.getName()+"/index.xml";
   DOMDocument* oldIndexDoc = loadDocument(old_iname);
   string iname = toDir.getName()+"/index.xml";
   DOMDocument* indexDoc = loadDocument(iname);

   const XMLCh* str = to_xml_ch_ptr("\n");
   DOMText* leader = indexDoc->createTextNode(str);

   const DOMNode* oldTimesteps = findNode("timesteps",
				    oldIndexDoc->getDocumentElement());
   DOMNode* ts;
   if (oldTimesteps != 0)
     ts = const_cast<DOMNode*>(findNode("timestep", oldTimesteps));

   // while we're at it, add restart information to index.xml
   if (maxTimestep >= 0)
     addRestartStamp(indexDoc, fromDir, maxTimestep);

   // create timesteps element if necessary
   DOMNode* timesteps = const_cast<DOMNode*>(findNode("timesteps",
				 indexDoc->getDocumentElement()));
   if (timesteps == 0) {

      const XMLCh* newline = to_xml_ch_ptr("\n");
      leader = indexDoc->createTextNode(newline);
      
      indexDoc->getDocumentElement()->appendChild(leader);

      str = to_xml_ch_ptr("timesteps");
      timesteps = indexDoc->createElement(str);
      
      indexDoc->getDocumentElement()->appendChild(timesteps);
   }
   
   int timestep;
   while (ts != 0) {
      get(ts, timestep);
      if (timestep > startTimestep &&
	  (timestep <= maxTimestep || maxTimestep < 0)) {
	 // copy the timestep directory over
	 DOMNamedNodeMap* attributes = ts->getAttributes();
	 
	 str = to_xml_ch_ptr("href");
	 DOMNode* hrefNode = attributes->getNamedItem(str);

	 if (hrefNode == 0)
	    throw InternalError("timestep href attribute not found");
	 char* href = const_cast<char*>(to_char_ptr(hrefNode->getNodeValue()));
	 strtok(href, "/"); // just grab the directory part
	 Dir timestepDir = fromDir.getSubdir(href);
	 if (removeOld)
	    timestepDir.move(toDir);
	 else
	    timestepDir.copy(toDir);

	 if (areCheckpoints)
	    d_checkpointTimestepDirs.push_back(toDir.getSubdir(href).getName());
	 
	 // add the timestep to the index.xml

	 str = to_xml_ch_ptr("\n\t");
	 leader = indexDoc->createTextNode(str);

	 timesteps->appendChild(leader);

	 str = to_xml_ch_ptr("timestep");
	 DOMElement* newTS = indexDoc->createElement(str);

	 ostringstream timestep_str;
	 timestep_str << timestep;

	 str = to_xml_ch_ptr(timestep_str.str().c_str());
	 DOMText* value = indexDoc->createTextNode(str);

	 newTS->appendChild(value);
	 for (unsigned int i = 0; i < attributes->getLength(); i++)
	    newTS->setAttribute(attributes->item(i)->getNodeName(),
			       attributes->item(i)->getNodeValue());
	 
	 timesteps->appendChild(newTS); // copy to new index.xml

	 str = to_xml_ch_ptr("\n");
	 DOMText* trailer = indexDoc->createTextNode(str);

	 timesteps->appendChild(trailer);
      }
      ts = const_cast<DOMNode*>(findNextNode("timestep", ts));
   }

   ofstream copiedIndex(iname.c_str());
   copiedIndex << indexDoc << endl;
   indexDoc->release();

   // we don't need the old document anymore...
   oldIndexDoc->release();

}

void DataArchiver::copyDatFiles(Dir& fromDir, Dir& toDir, int startTimestep,
				int maxTimestep, bool removeOld)
{
   char buffer[1000];

   // find the dat file via the globals block in index.xml
   string iname = fromDir.getName()+"/index.xml";
   DOMDocument* indexDoc = loadDocument(iname);

   const DOMNode* globals = findNode("globals", indexDoc->getDocumentElement());
   if (globals != 0) {
      DOMNode* variable = const_cast<DOMNode*>(findNode("variable", globals));
      while (variable != 0) {
	 DOMNamedNodeMap* attributes = variable->getAttributes();

	 const XMLCh* str = to_xml_ch_ptr("href");
	 DOMNode* hrefNode = attributes->getNamedItem(str);

	 if (hrefNode == 0)
	    throw InternalError("global variable href attribute not found");
	 const char* href = to_char_ptr(hrefNode->getNodeValue());
	 
	 // copy up to maxTimestep lines of the old dat file to the copy
	 ifstream datFile((fromDir.getName()+"/"+href).c_str());
	 ofstream copyDatFile((toDir.getName()+"/"+href).c_str(), ios::app);
	 int timestep = startTimestep;
	 while (datFile.getline(buffer, 1000) &&
		(timestep < maxTimestep || maxTimestep < 0)) {
	    copyDatFile << buffer << endl;
	    timestep++;
	 }
	 datFile.close();

	 if (removeOld) 
	    fromDir.remove(href);
	 
	 variable = const_cast<DOMNode*>(findNextNode("variable", variable));
      }
   }
   indexDoc->release();
}

void DataArchiver::createIndexXML(Dir& dir)
{
   const XMLCh* str = to_xml_ch_ptr("Core");
   DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(str);

   str = to_xml_ch_ptr("Uintah_DataArchive");
   DOMDocument* doc = impl->createDocument(0, str, 0);
   DOMElement* rootElem = doc->getDocumentElement();

   str = to_xml_ch_ptr("numberOfProcessors");
   appendElement(rootElem, doc->createTextNode(str), d_myworld->size());

   str = to_xml_ch_ptr("Meta");
   DOMElement* metaElem = doc->createElement(str);
   rootElem->appendChild(metaElem);
   
   str = to_xml_ch_ptr("username");
   appendElement(metaElem, doc->createTextNode(str), getenv("LOGNAME"));

   time_t t = time(NULL) ;
   
   str = to_xml_ch_ptr("date");
   appendElement(metaElem, doc->createTextNode(str), ctime(&t));

   str = to_xml_ch_ptr("endianness"); 
   appendElement(metaElem, doc->createTextNode(str), endianness().c_str());

   str = to_xml_ch_ptr("nBits");
   appendElement(metaElem, doc->createTextNode(str), (int)sizeof(unsigned long) * 8 );
   
   string iname = dir.getName()+"/index.xml";
   ofstream out(iname.c_str());
   out << doc << endl;
   doc->release();
}

void DataArchiver::finalizeTimestep(double time, double delt,
				    const GridP& grid, SchedulerP& sched)
{
  static bool wereSavesAndCheckpointsInitialized = false;
  dbg << "DataArchiver finalizeTimestep, delt= " << delt << endl;
  d_currentTime=time+delt;

  if (!wereSavesAndCheckpointsInitialized &&
      !(delt == 0) /* skip the initialization timestep for this
		      because it needs all computes to be set
		      to find the save labels */) {
    
    // This assumes that the TaskGraph doesn't change after the second
    // timestep and will need to change if the TaskGraph becomes dynamic. 
    wereSavesAndCheckpointsInitialized = true;
    
    if (d_outputInterval != 0.0 || d_outputTimestepInterval != 0) {
      initSaveLabels(sched);
      indexAddGlobals(); /* add saved global (reduction)
			    variables to index.xml */
    }

    if (d_checkpointInterval != 0.0 || d_checkpointTimestepInterval != 0 || 
	d_checkpointWalltimeInterval != 0)
      initCheckpoints(sched);
  }
  
  bool do_output=false;
  if ((d_outputInterval != 0.0 || d_outputTimestepInterval != 0) && delt != 0) {
    // Schedule task to dump out reduction variables at every timestep
    Task* t = scinew Task("DataArchiver::outputReduction",
			  this, &DataArchiver::outputReduction);
    
    for(int i=0;i<(int)d_saveReductionLabels.size();i++) {
      SaveItem& saveItem = d_saveReductionLabels[i];
      const VarLabel* var = saveItem.label_;
      const MaterialSubset* matls = saveItem.getMaterialSet()->getUnion();
      t->requires(Task::NewDW, var, matls);
    }
    
    sched->addTask(t, 0, 0);
    
    dbg << "Created reduction variable output task" << endl;
    if (d_wasOutputTimestep){
      scheduleOutputTimestep(d_dir, d_saveLabels, grid, sched);
      do_output=true;
    }
  }
    
  if (d_wasCheckpointTimestep){
    // output checkpoint timestep
    Task* t = scinew Task("DataArchiver::outputCheckpointReduction",
			  this, &DataArchiver::outputCheckpointReduction);
    
    for(int i=0;i<(int)d_checkpointReductionLabels.size();i++) {
      SaveItem& saveItem = d_checkpointReductionLabels[i];
      const VarLabel* var = saveItem.label_;
      const MaterialSubset* matls = saveItem.getMaterialSet()->getUnion();
      t->requires(Task::NewDW, var, matls);
    }
    sched->addTask(t, 0, 0);
    
    dbg << "Created checkpoint reduction variable output task" << endl;

    scheduleOutputTimestep(d_checkpointsDir, d_checkpointLabels,
			   grid, sched);
    do_output=true;
  }
  if(do_output && delt == 0)
    beginOutputTimestep(time, delt, grid);
}


void DataArchiver::beginOutputTimestep(double time, double delt,
				       const GridP& grid)
{
  d_currentTime=time+delt;
  dbg << "beginOutputTimestep called at time=" << d_currentTime << '\n';
  if (d_outputInterval != 0.0 && delt != 0) {
    if(d_currentTime >= d_nextOutputTime) {
      // output timestep
      d_wasOutputTimestep = true;
      outputTimestep(d_dir, d_saveLabels, time, delt, grid,
		     &d_lastTimestepLocation);
      while (d_currentTime >= d_nextOutputTime)
	d_nextOutputTime+=d_outputInterval;
    }
    else
      d_wasOutputTimestep = false;
  }
  else if (d_outputTimestepInterval != 0 && delt != 0) {
    if(d_currentTimestep >= d_nextOutputTimestep) {
      // output timestep
      d_wasOutputTimestep = true;
      outputTimestep(d_dir, d_saveLabels, time, delt, grid,
		     &d_lastTimestepLocation);
      while (d_currentTimestep >= d_nextOutputTimestep)
	d_nextOutputTimestep+=d_outputTimestepInterval;
    }
    else
      d_wasOutputTimestep = false;
  }
  
  if ((d_checkpointInterval != 0.0 && d_currentTime >= d_nextCheckpointTime) ||
      (d_checkpointTimestepInterval != 0 &&
       d_currentTimestep >= d_nextCheckpointTimestep) ||
      (d_checkpointWalltimeInterval != 0 &&
       Time::currentSeconds() >= d_nextCheckpointWalltime)) {
    d_wasCheckpointTimestep=true;
    string timestepDir;
    outputTimestep(d_checkpointsDir, d_checkpointLabels, time, delt,
		   grid, &timestepDir,
		   d_checkpointReductionLabels.size() > 0);
    
    string iname = d_checkpointsDir.getName()+"/index.xml";
    DOMDocument* index;
    
    if (d_writeMeta) {
      index = loadDocument(iname);
      
      // store a back up in case it dies while writing index.xml
      string ibackup_name = d_checkpointsDir.getName()+"/index_backup.xml";
      ofstream index_backup(ibackup_name.c_str());
      index_backup << index << endl;
    }

    d_checkpointTimestepDirs.push_back(timestepDir);
    if ((int)d_checkpointTimestepDirs.size() > d_checkpointCycle) {
      if (d_writeMeta) {
	// remove reference to outdated checkpoint directory from the
	// checkpoint index
	DOMNode* ts = const_cast<DOMNode*>(findNode("timesteps", index->getDocumentElement()));
	DOMNode* removed;
	do {
	  DOMNode* temp = ts->getFirstChild();
	  removed = ts->removeChild(temp);
	} while (removed->getNodeType() != DOMNode::ELEMENT_NODE);
	ofstream indexout(iname.c_str());
	indexout << index << endl;
	
	// remove out-dated checkpoint directory
	Dir expiredDir(d_checkpointTimestepDirs.front());
	expiredDir.forceRemove();
      }
      d_checkpointTimestepDirs.pop_front();
    }
    if (d_checkpointInterval != 0.0) {
      while (d_currentTime >= d_nextCheckpointTime)
	d_nextCheckpointTime += d_checkpointInterval;
    }
    else if (d_checkpointTimestepInterval != 0) {
      while (d_currentTimestep >= d_nextCheckpointTimestep)
	d_nextCheckpointTimestep += d_checkpointTimestepInterval;
    }
    if (d_checkpointWalltimeInterval != 0) {
      while (Time::currentSeconds() >= d_nextCheckpointWalltime)
	d_nextCheckpointWalltime += d_checkpointWalltimeInterval;
    }

    if (d_writeMeta)
      index->release();
  } else {
    d_wasCheckpointTimestep=false;
  }
}

void DataArchiver::outputTimestep(Dir& baseDir,
				  vector<DataArchiver::SaveItem>&,
				  double time, double delt,
				  const GridP& grid,
				  string* pTimestepDir /* passed back */,
				  bool hasGlobals /* = false */)
{
  int numLevels = grid->numLevels();
  int timestep = d_currentTimestep;
  
  const XMLCh* str;

  ostringstream tname;
  tname << "t" << setw(5) << setfill('0') << timestep;
  *pTimestepDir = baseDir.getName() + "/" + tname.str();

  // Create the directory for this timestep, if necessary
  if(d_writeMeta){
    Dir tdir;
    try {
      tdir = baseDir.createSubdir(tname.str());
      
      str = to_xml_ch_ptr("LS");
      DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(str);

      str = to_xml_ch_ptr("Uintah_timestep");
      DOMDocument* doc = impl->createDocument(0, str, 0);

      DOMElement* rootElem = doc->getDocumentElement();

      str = to_xml_ch_ptr("Time");
      DOMElement* timeElem = doc->createElement(str);
      rootElem->appendChild(timeElem);

      str = to_xml_ch_ptr("timestepNumber");
      appendElement(timeElem, doc->createTextNode(str), timestep);

      str = to_xml_ch_ptr("currentTime");
      appendElement(timeElem, doc->createTextNode(str), time+delt);

      str = to_xml_ch_ptr("delt");
      appendElement(timeElem, doc->createTextNode(str), delt);
      
      str = to_xml_ch_ptr("Grid");
      DOMElement* gridElem = doc->createElement(str);
      rootElem->appendChild(gridElem);
      
      appendElement(gridElem, doc->createTextNode(to_xml_ch_ptr("numLevels")), numLevels);
      for(int l = 0;l<numLevels;l++){
	LevelP level = grid->getLevel(l);
	DOMElement* levelElem = doc->createElement(to_xml_ch_ptr("Level"));
	gridElem->appendChild(levelElem);

	if (level->getPeriodicBoundaries() != IntVector(0,0,0))
	  appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("periodic")), level->getPeriodicBoundaries());
	appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("numPatches")), level->numPatches());
	appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("totalCells")), level->totalCells());
	appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("cellspacing")), level->dCell());
	appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("anchor")), level->getAnchor());
	appendElement(levelElem, doc->createTextNode(to_xml_ch_ptr("id")), level->getID());
	Level::const_patchIterator iter;
	for(iter=level->patchesBegin(); iter != level->patchesEnd(); iter++){
	  const Patch* patch=*iter;
	  DOMElement* patchElem = doc->createElement(to_xml_ch_ptr("Patch"));
	  levelElem->appendChild(patchElem);
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("id")), patch->getID());
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("lowIndex")), patch->getCellLowIndex());
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("highIndex")), patch->getCellHighIndex());
	  Box box = patch->getBox();
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("lower")), box.lower());
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("upper")), box.upper());
	  appendElement(patchElem, doc->createTextNode(to_xml_ch_ptr("totalCells")), patch->totalCells());
	}
      }
      DOMElement* dataElem = doc->createElement(to_xml_ch_ptr("Data"));
      rootElem->appendChild(dataElem);
      for(int l=0;l<numLevels;l++){
	ostringstream lname;
	lname << "l" << l;
	for(int i=0;i<d_myworld->size();i++){
	  ostringstream pname;
	  pname << lname.str() << "/p" << setw(5) << setfill('0') << i << ".xml";
	  DOMText* leader = doc->createTextNode(to_xml_ch_ptr("\n\t"));
	  dataElem->appendChild(leader);
	  DOMElement* df = doc->createElement(to_xml_ch_ptr("Datafile"));
	  dataElem->appendChild(df);
	  df->setAttribute(to_xml_ch_ptr("href"), 
			   to_xml_ch_ptr2(pname.str().c_str()));
	  ostringstream procID;
	  procID << i;
	  df->setAttribute(to_xml_ch_ptr("proc"), 
			   to_xml_ch_ptr2(procID.str().c_str()));
	  ostringstream labeltext;
	  labeltext << "Processor " << i << " of " << d_myworld->size();
	  DOMText* label = doc->createTextNode(to_xml_ch_ptr(labeltext.str().c_str()));
	  df->appendChild(label);
	  DOMText* trailer = doc->createTextNode(to_xml_ch_ptr("\n"));
	  dataElem->appendChild(trailer);
	}
      }
	 
      if (hasGlobals) {
	DOMText* leader = doc->createTextNode(to_xml_ch_ptr("\n\t"));
	dataElem->appendChild(leader);
	DOMElement* df = doc->createElement(to_xml_ch_ptr("Datafile"));
	dataElem->appendChild(df);
	df->setAttribute(to_xml_ch_ptr("href"), to_xml_ch_ptr2("global.xml"));
	DOMText* trailer = doc->createTextNode(to_xml_ch_ptr("\n"));
	dataElem->appendChild(trailer);
      }
	 
      string name = tdir.getName()+"/timestep.xml";
      ofstream out(name.c_str());
      out << doc << endl;
      doc->release();
    } catch(ErrnoException& e) {
      if(e.getErrno() != EEXIST)
	throw;
      tdir = baseDir.getSubdir(tname.str());
    }
      
    // Create the directory for this level, if necessary
    for(int l=0;l<numLevels;l++){
      ostringstream lname;
      lname << "l" << l;
      Dir ldir;
      try {
	ldir = tdir.createSubdir(lname.str());
      } catch(ErrnoException& e) {
	if(e.getErrno() != EEXIST)
	  throw;
	ldir = tdir.getSubdir(lname.str());
      }
    }
  }
}
      
void DataArchiver::executedTimestep()
{
  vector<Dir*> baseDirs;
  if (d_wasOutputTimestep)
    baseDirs.push_back(&d_dir);
  if (d_wasCheckpointTimestep)
    baseDirs.push_back(&d_checkpointsDir);

  for (int i = 0; i < static_cast<int>(baseDirs.size()); i++) {
    int timestep = d_currentTimestep;
    
    ostringstream tname;
    tname << "t" << setw(5) << setfill('0') << timestep;
    
    // Reference this timestep in index.xml
    if(d_writeMeta){
      string iname = baseDirs[i]->getName()+"/index.xml";
      DOMDocument* indexDoc = loadDocument(iname);
      DOMNode* ts = const_cast<DOMNode*>(findNode("timesteps", indexDoc->getDocumentElement()));
      if(ts == 0){
	DOMText* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
	indexDoc->getDocumentElement()->appendChild(leader);
	ts = indexDoc->createElement(to_xml_ch_ptr("timesteps"));
	indexDoc->getDocumentElement()->appendChild(ts);
      }
      bool found=false;
      for(DOMNode* n = ts->getFirstChild(); n != 0; n=n->getNextSibling()){
	if(strcmp(to_char_ptr(n->getNodeName()),"timestep") == 0){
	  int readtimestep;
	  if(!get(n, readtimestep))
	    throw InternalError("Error parsing timestep number");
	  if(readtimestep == timestep){
	    found=true;
	    break;
	  }
	}
      }
      if(!found){
	string timestepindex = tname.str()+"/timestep.xml";      
	DOMText* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n\t"));
	ts->appendChild(leader);
	DOMElement* newElem = indexDoc->createElement(to_xml_ch_ptr("timestep"));
	ts->appendChild(newElem);
	ostringstream value;
	value << timestep;
	DOMText* newVal = indexDoc->createTextNode(to_xml_ch_ptr(value.str().c_str()));
	newElem->appendChild(newVal);
	newElem->setAttribute(to_xml_ch_ptr("href"), to_xml_ch_ptr2(timestepindex.c_str()));
	DOMText* trailer = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
	ts->appendChild(trailer);
      }
      
      ofstream indexOut(iname.c_str());
      indexOut << indexDoc << endl;

      indexDoc->release();
    }
  }
}

void
DataArchiver::scheduleOutputTimestep(Dir& baseDir,
				     vector<DataArchiver::SaveItem>& saveLabels,
				     const GridP& grid, SchedulerP& sched)
{
  // Schedule a bunch o tasks - one for each variable, for each patch
  int n=0;
  for(int i=0;i<grid->numLevels();i++){
    const LevelP& level = grid->getLevel(i);
    vector< SaveItem >::iterator saveIter;
    const PatchSet* patches = level->eachPatch();
    for(saveIter = saveLabels.begin(); saveIter!= saveLabels.end();
	saveIter++) {
      const MaterialSet* matls = (*saveIter).getMaterialSet();
      Task* t = scinew Task("DataArchiver::output", 
			    this, &DataArchiver::output,
			    &baseDir, (*saveIter).label_);
      t->requires(Task::NewDW, (*saveIter).label_, Ghost::None);
      sched->addTask(t, patches, matls);
      n++;
    }
  }
  dbg << "Created " << n << " output tasks\n";
}

DOMDocument* DataArchiver::loadDocument(string xmlName)
{
   // Instantiate the DOM parser.
   XercesDOMParser* parser = new XercesDOMParser;
   parser->setDoValidation(false);
   
   SimpleErrorHandler handler;
   parser->setErrorHandler(&handler);

   parser->parse(xmlName.c_str());
   
   if(handler.foundError)
     throw InternalError("Error reading file: " + xmlName);
   
   // make a clone because the document is owned by the parser
   DOMDocument* doc = dynamic_cast<DOMDocument*>(parser->getDocument()->cloneNode(true));
   delete parser;

   return doc;
}

const string
DataArchiver::getOutputLocation() const
{
    return d_dir.getName();
}

void DataArchiver::indexAddGlobals()
{
  // assume for now that global variables that get computed will not
  // change from timestep to timestep
  static bool wereGlobalsAdded = false;
   if (d_writeMeta && !wereGlobalsAdded) {
     wereGlobalsAdded = true;
     // add saved global (reduction) variables to index.xml
      string iname = d_dir.getName()+"/index.xml";
      DOMDocument* indexDoc = loadDocument(iname);
      DOMNode* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
      indexDoc->getDocumentElement()->appendChild(leader);
      DOMNode* globals = indexDoc->createElement(to_xml_ch_ptr("globals"));
      indexDoc->getDocumentElement()->appendChild(globals);
      for (vector<SaveItem>::iterator iter = d_saveReductionLabels.begin();
           iter != d_saveReductionLabels.end(); iter++) {
         SaveItem& saveItem = *iter;
         const VarLabel* var = saveItem.label_;
         const MaterialSubset* matls = saveItem.getMaterialSet()->getUnion();
         for (int m = 0; m < matls->size(); m++) {
            int matlIndex = matls->get(m);
            ostringstream href;
            href << var->getName();
            if (matlIndex < 0)
	      href << ".dat\0";
            else
	      href << "_" << matlIndex << ".dat\0";
            DOMElement* newElem = indexDoc->createElement(to_xml_ch_ptr("variable"));
            DOMText* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n\t"));
            DOMText* trailer = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
            globals->appendChild(leader);
            globals->appendChild(newElem);
            globals->appendChild(trailer);
            newElem->setAttribute(to_xml_ch_ptr("href"), 
				  to_xml_ch_ptr2(href.str().c_str()));
            newElem->setAttribute(to_xml_ch_ptr("type"),
				  to_xml_ch_ptr2(var->typeDescription()->getName().c_str()));
            newElem->setAttribute(to_xml_ch_ptr("name"), to_xml_ch_ptr2(var->getName().c_str()));
         }
      }

      ofstream indexOut(iname.c_str());
      indexOut << indexDoc << endl;
      indexDoc->release();
   }
}

void DataArchiver::outputReduction(const ProcessorGroup*,
				   const PatchSubset* pss,
				   const MaterialSubset* /*matls*/,
				   DataWarehouse* /*old_dw*/,
				   DataWarehouse* new_dw)
{
  // Dump the stuff in the reduction saveset into files in the uda
  dbg << "DataArchiver::outputReduction called\n";

  for(int i=0;i<(int)d_saveReductionLabels.size();i++) {
    SaveItem& saveItem = d_saveReductionLabels[i];
    const VarLabel* var = saveItem.label_;
    const MaterialSubset* matls = saveItem.getMaterialSet()->getUnion();
    for (int m = 0; m < matls->size(); m++) {
      int matlIndex = matls->get(m);
      dbg << "Reduction matl " << matlIndex << endl;
      ostringstream filename;
      filename << d_dir.getName() << "/" << var->getName();
      if (matlIndex < 0)
	filename << ".dat\0";
      else
	filename << "_" << matlIndex << ".dat\0";
	  
#ifdef __GNUG__
      ofstream out(filename.str().c_str(), ios::app);
#else
      ofstream out(filename.str().c_str(), ios_base::app);
#endif
      out << setprecision(17) << d_currentTime << "\t";
      new_dw->print(out, var, 0, matlIndex);
      out << "\n";
    }
  }
}

void DataArchiver::outputCheckpointReduction(const ProcessorGroup* world,
					     const PatchSubset*,
					     const MaterialSubset* /*matls*/,
					     DataWarehouse* old_dw,
					     DataWarehouse* new_dw)
{
  dbg << "DataArchiver::outputCheckpointReduction called\n";
  // Dump the stuff in the reduction saveset into files in the uda
  for(int i=0;i<(int)d_checkpointReductionLabels.size();i++) {
    SaveItem& saveItem = d_checkpointReductionLabels[i];
    const VarLabel* var = saveItem.label_;
    const MaterialSubset* matls = saveItem.getMaterialSet()->getUnion();
    PatchSubset* patches = scinew PatchSubset(0);
    patches->add(0);
    output(world, patches, matls, old_dw, new_dw, &d_checkpointsDir, var);
    delete patches;
  }
}

void DataArchiver::output(const ProcessorGroup*,
			  const PatchSubset* patches,
			  const MaterialSubset* matls,
			  DataWarehouse* /*old_dw*/,
			  DataWarehouse* new_dw,
			  Dir* p_dir,
			  const VarLabel* var)
{
  bool isReduction = var->typeDescription()->isReductionVariable();

  dbg << "output called ";
  if(patches->size() == 1 && !patches->get(0)){
    dbg << "for reduction";
  } else {
    dbg << "on patches: ";
    for(int p=0;p<patches->size();p++){
      if(p != 0)
	dbg << ", ";
      if (patches->get(p) == 0)
	dbg << -1;
      else
	dbg << patches->get(p)->getID();
    }
  }
  dbg << ", variable: " << var->getName() << ", materials: ";
  for(int m=0;m<matls->size();m++){
    if(m != 0)
      dbg << ", ";
    dbg << matls->get(m);
  }
  dbg << " at time: " << d_currentTimestep << "\n";
  
  ostringstream tname;
  tname << "t" << setw(5) << setfill('0') << d_currentTimestep;
  
  Dir tdir = p_dir->getSubdir(tname.str());
  
  string xmlFilename;
  string dataFilebase;
  string dataFilename;
  if (!isReduction) {
    ostringstream lname;
    ASSERT(patches->size() != 0);
    ASSERT(patches->get(0) != 0);
    const Level* level = patches->get(0)->getLevel();
#if SCI_ASSERTION_LEVEL >= 1
    for(int i=0;i<patches->size();i++)
      ASSERT(patches->get(i)->getLevel() == level);
#endif
    lname << "l" << level->getIndex(); // Hard coded - steve
    Dir ldir = tdir.getSubdir(lname.str());
    
    ostringstream pname;
    pname << "p" << setw(5) << setfill('0') << d_myworld->myrank();
    xmlFilename = ldir.getName() + "/" + pname.str() + ".xml";
    dataFilebase = pname.str() + ".data";
    dataFilename = ldir.getName() + "/" + dataFilebase;
  } else {
    xmlFilename =  tdir.getName() + "/global.xml";
    dataFilebase = "global.data";
    dataFilename = tdir.getName() + "/" + dataFilebase;
  }

  // Not only lock to prevent multiple threads from writing over the same
  // file, but also lock because xerces (DOM..) has thread-safety issues.
 d_outputLock.lock(); 
 { // make sure doc's constructor is called after the lock.
  DOMDocument* doc; 
  ifstream test(xmlFilename.c_str());
  if(test){
    // Instantiate the DOM parser.
    XercesDOMParser* parser = new XercesDOMParser;
    parser->setDoValidation(false);
    
    SimpleErrorHandler handler;
    parser->setErrorHandler(&handler);
    
    // Parse the input file
    // No exceptions just yet, need to add
    
    parser->parse(xmlFilename.c_str());
    
    if(handler.foundError)
      throw InternalError("Error reading file: "+xmlFilename);
    
    // Add the parser contents to the ProblemSpecP d_doc
    doc = dynamic_cast<DOMDocument*>(parser->getDocument()->cloneNode(true));
    delete parser;
  } else {
    DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(to_xml_ch_ptr("LS"));;
    
    doc = impl->createDocument(0,to_xml_ch_ptr("Uintah_Output"),
			      0);
  }

  // Find the end of the file
  DOMNode* n = doc->getDocumentElement();
  ASSERT(n != NULL);
  n = const_cast<DOMNode*>(findNode("Variable", n));
   
  long cur=0;
  while(n != NULL){
    DOMNode* endNode = const_cast<DOMNode*>(findNode("end", n));
    ASSERT(endNode != 0);
    const DOMNode* tn = findTextNode(endNode);
    //DOMString val = tn->getNodeValue();
    const char* s = to_char_ptr(tn->getNodeValue());
    long end = atol(s);
    
    if(end > cur)
      cur=end;
    n = const_cast<DOMNode*>(findNextNode("Variable", n));
  }


  DOMElement* rootElem = doc->getDocumentElement();
  // Open the data file
  int fd = open(dataFilename.c_str(), O_WRONLY|O_CREAT, 0666);
  if(fd == -1){
    cerr << "Cannot open dataFile: " << dataFilename << '\n';
    throw ErrnoException("DataArchiver::output (open call)", errno);
  }

  struct stat st;
  int s = fstat(fd, &st);
  if(s == -1){
    cerr << "Cannot fstat: " << dataFilename << '\n';
    throw ErrnoException("DataArchiver::output (stat call)", errno);
  }
  ASSERTEQ(cur, st.st_size);

  for(int p=0;p<patches->size();p++){
    const Patch* patch = patches->get(p);
    int patchID = patch?patch->getID():-1;
    for(int m=0;m<matls->size();m++){
      int matlIndex = matls->get(m);
      DOMElement* pdElem = doc->createElement(to_xml_ch_ptr("Variable"));
      rootElem->appendChild(pdElem);
      DOMText* tailer = doc->createTextNode(to_xml_ch_ptr("\n"));
      rootElem->appendChild(tailer);
  
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("variable")), var->getName());
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("index")), matlIndex);
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("patch")), patchID);
      pdElem->setAttribute(to_xml_ch_ptr("type"), 
			   to_xml_ch_ptr2(var->typeDescription()->getName().c_str()));

#ifdef __sgi
      off64_t ls = lseek64(fd, cur, SEEK_SET);
#else
      off_t ls = lseek(fd, cur, SEEK_SET);
#endif
      if(ls == -1)
	throw ErrnoException("DataArchiver::output (lseek64 call)", errno);

      // Pad appropriately
      if(cur%PADSIZE != 0){
	long pad = PADSIZE-cur%PADSIZE;
	char* zero = scinew char[pad];
	bzero(zero, pad);
	write(fd, zero, pad);
	cur+=pad;
	delete[] zero;
      }
      ASSERTEQ(cur%PADSIZE, 0);
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("start")), cur);

      OutputContext oc(fd, cur, pdElem);
      new_dw->emit(oc, var, matlIndex, patch);
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("end")), oc.cur);
      appendElement(pdElem, doc->createTextNode(to_xml_ch_ptr("filename")), dataFilebase.c_str());
      s = fstat(fd, &st);
      if(s == -1)
	throw ErrnoException("DataArchiver::output (stat call)", errno);
      ASSERTEQ(oc.cur, st.st_size);
      cur=oc.cur;
    }
  }

  s = close(fd);
  if(s == -1)
    throw ErrnoException("DataArchiver::output (close call)", errno);

  ofstream out(xmlFilename.c_str());
  out << doc << endl;
  doc->release();

  if(d_writeMeta){
    // Rewrite the index if necessary...
    string iname = p_dir->getName()+"/index.xml";
    DOMDocument* indexDoc = loadDocument(iname);
    
    DOMNode* vs;
    string variableSection = (isReduction) ? "globals" : "variables";
	 
    vs = const_cast<DOMNode*>(findNode(variableSection, indexDoc->getDocumentElement()));
    if(vs == 0){
      DOMText* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
      indexDoc->getDocumentElement()->appendChild(leader);
      vs = indexDoc->createElement(to_xml_ch_ptr(variableSection.c_str()));
      indexDoc->getDocumentElement()->appendChild(vs);
    }
    bool found=false;
    for(DOMNode* n = vs->getFirstChild(); n != 0; n=n->getNextSibling()){
      if(strcmp(to_char_ptr(n->getNodeName()),"variable") == 0) {
	DOMNamedNodeMap* attributes = n->getAttributes();
	DOMNode* varname = attributes->getNamedItem(to_xml_ch_ptr("name"));
	if(varname == 0)
	  throw InternalError("varname not found");
	string vn = to_char_ptr(varname->getNodeValue());
	if(vn == var->getName()){
	  found=true;
	  break;
	}
      }
    }
    if(!found){
      DOMText* leader = indexDoc->createTextNode(to_xml_ch_ptr("\n\t"));
      vs->appendChild(leader);
      DOMElement* newElem = indexDoc->createElement(to_xml_ch_ptr("variable"));
      vs->appendChild(newElem);
      newElem->setAttribute(to_xml_ch_ptr("type"), 
			    to_xml_ch_ptr2(var->typeDescription()->getName().c_str()));
      newElem->setAttribute(to_xml_ch_ptr("name"), to_xml_ch_ptr2(var->getName().c_str()));
      DOMText* trailer = indexDoc->createTextNode(to_xml_ch_ptr("\n"));
      vs->appendChild(trailer);
    }

    ofstream indexOut(iname.c_str());
    indexOut << indexDoc << endl;  
    indexDoc->release();
  }
 }
 d_outputLock.unlock(); 
}

static
Dir
makeVersionedDir(const std::string nameBase)
{
  unsigned int dirMin = 0;
  unsigned int dirNum = 0;
  unsigned int dirMax = 0;

  bool dirCreated = false;

  // My (Dd) understanding of this while loop is as follows: We try to
  // make a new directory starting at 0 (000), then 1 (001), and then
  // doubling the number each time that the directory already exists.
  // Once we find a directory number that does not exist, we start
  // back at the minimum number that we had already tried and work
  // forward to find an actual directory number that we can use.
  //
  // Eg: If 001 and 002 already exist, then the algorithm tries to
  // create 001, fails (it already exists), tries to created 002,
  // fails again, tries to create 004, succeeds, deletes 004 because
  // we aren't sure that it is the smallest new number, creates 003,
  // succeeds, and since 003 is the smallest (dirMin) the algorithm
  // stops.
  //
  // This routine has been re-written to not use the Dir class because
  // the Dir class throws exceptions and exceptions (under SGI CC) add
  // a huge memory penalty in one (same penalty if more than) are
  // thrown.  This causes memory usage to change if the very first
  // directory (000) can be created (because no exception is thrown
  // and thus no memory is allocated for exceptions).
  //
  // If there is a real error, then we can throw an exception becuase
  // we don't care about the memory penalty.

  string dirName;

  while (!dirCreated) {
    ostringstream name;
    name << nameBase << "." << setw(3) << setfill('0') << dirNum;
    dirName = name.str();
      
    int code = mkdir( dirName.c_str(), 0777 );
    if( code == 0 ) // Created the directory successfully
      {
	dirMax = dirNum;
	if (dirMax == dirMin)
	  dirCreated = true;
	else
	  {
	    int code = rmdir( dirName.c_str() );
	    if (code != 0)
	      throw ErrnoException("DataArchiver.cc: rmdir failed", errno);
	  }
      }
    else
      {
	if( errno != EEXIST )
	  {
	    cerr << "makeVersionedDir: Error " << errno << " in mkdir\n";
	    throw ErrnoException("DataArchiver.cc: mkdir failed for some "
				 "reason besides dir already exists", errno);
	  }
	dirMin = dirNum + 1;
      }

    if (!dirCreated) {
      if (dirMax == 0) {
	if (dirNum == 0)
	  dirNum = 1;
	else
	  dirNum *= 2;
      } else {
	dirNum = dirMin + ((dirMax - dirMin) / 2);
      }
    }
  }
  // Move the symbolic link to point to the new version. We need to be careful
  // not to destroy data, so we only create the link if no file/dir by that
  // name existed or if it's already a link.
  bool make_link = false;
  struct stat sb;
  int rc = lstat(nameBase.c_str(), &sb);
  if ((rc != 0) && (errno == ENOENT))
    make_link = true;
  else if ((rc == 0) && (S_ISLNK(sb.st_mode))) {
    unlink(nameBase.c_str());
    make_link = true;
  }
  if (make_link)
    symlink(dirName.c_str(), nameBase.c_str());
   
  return Dir(dirName);
}

void  DataArchiver::initSaveLabels(SchedulerP& sched)
{
  dbg << "initSaveLabels called\n";
   SaveItem saveItem;
   d_saveReductionLabels.clear();
   d_saveLabels.clear();
  
   d_saveLabels.reserve(d_saveLabelNames.size());
   Scheduler::VarLabelMaterialMap* pLabelMatlMap;
   pLabelMatlMap = sched->makeVarLabelMaterialMap();
   for (list<SaveNameItem>::iterator it = d_saveLabelNames.begin();
        it != d_saveLabelNames.end(); it++) {
     
      VarLabel* var = VarLabel::find((*it).labelName);
      if (var == NULL)
         throw ProblemSetupException((*it).labelName +
				     " variable not found to save.");

      if ((*it).compressionMode != "")
	var->setCompressionMode((*it).compressionMode);
      
      Scheduler::VarLabelMaterialMap::iterator found =
	pLabelMatlMap->find(var->getName());

      if (found == pLabelMatlMap->end())
         throw ProblemSetupException((*it).labelName +
				  " variable not computed for saving.");
      
      saveItem.label_ = var;
      ConsecutiveRangeSet matlsToSave =
	(ConsecutiveRangeSet((*found).second)).intersected((*it).matls);
      saveItem.setMaterials(matlsToSave, prevMatls_, prevMatlSet_);
      
      if (((*it).matls != ConsecutiveRangeSet::all) &&
	  ((*it).matls != matlsToSave)) {
         throw ProblemSetupException((*it).labelName +
	  " variable not computed for all materials specified to save.");
      }
      
      if (saveItem.label_->typeDescription()->isReductionVariable()) {
	d_saveReductionLabels.push_back(saveItem);
      }
      else
         d_saveLabels.push_back(saveItem);
   }
   d_saveLabelNames.clear();
   delete pLabelMatlMap;
}


void DataArchiver::initCheckpoints(SchedulerP& sched)
{
   typedef vector<const Task::Dependency*> dep_vector;
   const dep_vector& initreqs = sched->getInitialRequires();
   SaveItem saveItem;
   d_checkpointReductionLabels.clear();
   d_checkpointLabels.clear();

   map< string, ConsecutiveRangeSet > label_matl_map;

   for (dep_vector::const_iterator iter = initreqs.begin();
	iter != initreqs.end(); iter++) {
      const Task::Dependency* dep = *iter;
      ConsecutiveRangeSet matls;
      const MaterialSubset* matSubset = (dep->matls != 0) ?
	dep->matls : dep->task->getMaterialSet()->getUnion();

      // The matSubset is assumed to be in ascending order or
      // addInOrder will throw an exception.
      matls.addInOrder(matSubset->getVector().begin(),
		       matSubset->getVector().end());
      ConsecutiveRangeSet& unionedVarMatls =
	label_matl_map[dep->var->getName()];
      unionedVarMatls = unionedVarMatls.unioned(matls);      
   }
         
   d_checkpointLabels.reserve(label_matl_map.size());
   map< string, ConsecutiveRangeSet >::iterator mapIter;
   bool hasDelT = false;
   for (mapIter = label_matl_map.begin();
        mapIter != label_matl_map.end(); mapIter++) {
      VarLabel* var = VarLabel::find((*mapIter).first);
      if (var == NULL)
         throw ProblemSetupException((*mapIter).first +
				  " variable not found to checkpoint.");

      saveItem.label_ = var;
      saveItem.setMaterials((*mapIter).second, prevMatls_, prevMatlSet_);

      if (string(var->getName()) == "delT") {
	hasDelT = true;
      }

      if (saveItem.label_->typeDescription()->isReductionVariable())
         d_checkpointReductionLabels.push_back(saveItem);
      else
         d_checkpointLabels.push_back(saveItem);
   }

   if (!hasDelT) {
     VarLabel* var = VarLabel::find("delT");
     if (var == NULL)
       throw ProblemSetupException("delT variable not found to checkpoint.");
     saveItem.label_ = var;
     ConsecutiveRangeSet globalMatl("-1");
     saveItem.setMaterials(globalMatl, prevMatls_, prevMatlSet_);
     ASSERT(saveItem.label_->typeDescription()->isReductionVariable());
     d_checkpointReductionLabels.push_back(saveItem);
   }
     
}

void DataArchiver::SaveItem::setMaterials(const ConsecutiveRangeSet& matls,
					  ConsecutiveRangeSet& prevMatls,
					  MaterialSetP& prevMatlSet)
{
  // reuse material sets when the same set of materials is used for different
  // SaveItems in a row -- easier than finding all reusable material set, but
  // effective in many common cases.
  if ((prevMatlSet != 0) && (matls == prevMatls)) {
    matlSet_ = prevMatlSet;
  }
  else {
    matlSet_ = scinew MaterialSet();
    vector<int> matlVec;
    matlVec.reserve(matls.size());
    for (ConsecutiveRangeSet::iterator iter = matls.begin();
	 iter != matls.end(); iter++)
      matlVec.push_back(*iter);
    matlSet_->addAll(matlVec);
    prevMatlSet = matlSet_;
    prevMatls = matls;
  }
}

bool DataArchiver::need_recompile(double time, double dt,
				  const GridP& grid)
{
  LevelP level = grid->getLevel(0);
  d_currentTime=time+dt;
  dbg << "DataArchiver::need_recompile called\n";
  d_currentTimestep++;
  bool recompile=false;
  bool do_output=false;
  if ((d_outputInterval != 0 && time+dt >= d_nextOutputTime) ||
      (d_outputTimestepInterval != 0 && d_currentTimestep+1 > d_nextOutputTimestep)){
    do_output=true;
    if(!d_wasOutputTimestep)
      recompile=true;
  } else {
    if(d_wasOutputTimestep)
      recompile=true;
  }
  if ((d_checkpointInterval != 0 && time+dt >= d_nextCheckpointTime) ||
      (d_checkpointTimestepInterval != 0 && d_currentTimestep+1 > d_nextCheckpointTimestep) ||
      (d_checkpointWalltimeInterval != 0 && Time::currentSeconds() >= d_nextCheckpointWalltime)) {
    do_output=true;
    if(!d_wasCheckpointTimestep)
      recompile=true;
  } else {
    if(d_wasCheckpointTimestep)
      recompile=true;
  }
  dbg << "wasOutputTimestep=" << d_wasOutputTimestep << '\n';
  dbg << "wasCheckpointTimestep=" << d_wasCheckpointTimestep << '\n';
  dbg << "time=" << time << '\n';
  dbg << "dt=" << dt << '\n';
  dbg << "do_output=" << do_output << '\n';
  if(do_output)
    beginOutputTimestep(time, dt, grid);
  else
    d_wasCheckpointTimestep=d_wasOutputTimestep=false;
  if(recompile)
    dbg << "We do request recompile\n";
  else
    dbg << "We do not request recompile\n";
  return recompile;
}

