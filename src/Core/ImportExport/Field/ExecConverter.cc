/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2004 Scientific Computing and Imaging Institute,
   University of Utah.

   License for the specific language governing rights and limitations under
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


/*
 *  ExecConverter.cc
 *
 *  Written by:
 *   Michael Callahan
 *   Department of Computer Science
 *   University of Utah
 *   December 2004
 *
 *  Copyright (C) 2004 SCI Institute
 */

// Use a standalone converter to do the field conversion into a
// temporary file, then read in that file.

#include <Core/ImportExport/Field/FieldIEPlugin.h>
#include <Core/Persistent/Pstreams.h>
#include <Core/Util/sci_system.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>

using namespace std;
using namespace SCIRun;


static void
Exec_setup_command(const char *cfilename, const string &precommand,
		   string &command, string &tmpfilename)
{
  // Filename as string
  const string filename(cfilename);
  
  // Base filename.
  string::size_type loc = filename.find_last_of("/");
  const string basefilename = filename.substr(loc);

  // Base filename with first extension removed.
  loc = basefilename.find_last_of(".");
  const string basenoext = basefilename.substr(0, loc);

  // Temporary filename.
  tmpfilename = "/tmp/" + basenoext + "-" + "123456" + ".fld";

  // Filename with first extension removed.
  loc = filename.find_last_of(".");
  const string filenoext = filename.substr(0, loc);

  // Replace all of the variables in the reader command.
  command = precommand;
  while ((loc = command.find("%f")) != string::npos)
  {
    command.replace(loc, 2, filename);
  }
  while ((loc = command.find("%e")) != string::npos)
  {
    command.replace(loc, 2, basenoext);
  }
  while ((loc = command.find("%t")) != string::npos)
  {
    command.replace(loc, 2, tmpfilename);
  }
}


static bool
Exec_execute_command(const string &icommand, const string &tmpfilename)
{
  cerr << "ExecConverter - Executing: " << icommand << endl;

  FILE *pipe = 0;
  bool result = true;
#ifdef __sgi
  string command = icommand + " 2>&1";
  pipe = popen(command.c_str(), "r");
  if (pipe == NULL)
  {
    cerr << "ExecConverter syscal error, command was: '" << command << "'\n";
    result = false;
  }
#else
  string command = icommand + " > " + tmpfilename + ".log 2>&1";
  const int status = sci_system(command.c_str());
  if (status != 0)
  {
    cerr << "ExecConverter syscal error " << status << ": "
	 << "command was '" << command << "'" << endl;
    result = false;
  }
  pipe = fopen((tmpfilename + ".log").c_str(), "r");
#endif

  char buffer[256];
  while (pipe && fgets(buffer, 256, pipe) != NULL)
  {
    cerr << buffer;
  }

#ifdef __sgi
  if (pipe) { pclose(pipe); }
#else
  if (pipe)
  {
    fclose(pipe);
    unlink((tmpfilename + ".log").c_str());
  }
#endif

  return result;
}


static FieldHandle
Exec_reader(ProgressReporter *pr, const char *cfilename, const string &precommand)
{
  string command, tmpfilename;
  Exec_setup_command(cfilename, precommand, command, tmpfilename);

  if (Exec_execute_command(command, tmpfilename))
  {
    Piostream *stream = auto_istream(tmpfilename);
    if (!stream)
    {
      cerr << "Error reading converted file '" + tmpfilename + "'." << endl;
      return 0;
    }
    
    // Read the file
    FieldHandle field;
    Pio(*stream, field);

    cerr << "ExecConverter - Successfully converted " << cfilename << endl;

    unlink(tmpfilename.c_str());
    
    return field;
  }

  unlink(tmpfilename.c_str());
  return 0;
}



static bool
Exec_writer(ProgressReporter *pr,
	    FieldHandle field, const char *cfilename, const string &precommand)
{
  string command, tmpfilename;
  bool result = true;

  Exec_setup_command(cfilename, precommand, command, tmpfilename);

  Piostream *stream = scinew BinaryPiostream(tmpfilename, Piostream::Write);
  if (stream->error())
  {
    cerr << "Could not open temporary file '" + tmpfilename +
      "' for writing." << endl;
    result = false;
  }
  else
  {
    Pio(*stream, field);
    
    result = Exec_execute_command(command, tmpfilename);
  }
  unlink(tmpfilename.c_str());
  delete stream;

  return result;
}



// CurveField

static FieldHandle
TextCurveField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToCurveFieldToText %e.pts %e.edges %t";
  return Exec_reader(pr, filename, command);
}

static bool
TextCurveField_writer(ProgressReporter *pr,
		      FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "CurveFieldToText %t %e.pts %e.edges";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextCurveField_plugin("TextCurveField",
		      "{.pts} {.edges}", "",
		      TextCurveField_reader,
		      TextCurveField_writer);


// HexVolField

static FieldHandle
TextHexVolField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToHexVolField %e.pts %e.hexes %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextHexVolField_writer(ProgressReporter *pr,
		       FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "HexVolFieldToText %t %e.pts %e.hexes";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextHexVolField_plugin("TextHexVolField",
		       "", "",
		       TextHexVolField_reader,
		       TextHexVolField_writer);


// QuadSurfField

static FieldHandle
TextQuadSurfField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToQuadSurfField %e.pts %e.quads %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextQuadSurfField_writer(ProgressReporter *pr,
			 FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "QuadSurfFieldToText %t %e.pts %e.quads";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextQuadSurfField_plugin("TextQuadSurfField",
			 "", "",
			 TextQuadSurfField_reader,
			 TextQuadSurfField_writer);


// TetVolField

static FieldHandle
TextTetVolField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToTetVolField %e.pts %e.tets %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextTetVolField_writer(ProgressReporter *pr,
		       FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TetVolFieldToText %t %e.pts %e.tets";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextTetVolField_plugin("TextTetVolField",
		       "", "",
		       TextTetVolField_reader,
		       TextTetVolField_writer);


// TriSurfField

static FieldHandle
TextTriSurfField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToTriSurfField %e.pts %e.tets %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextTriSurfField_writer(ProgressReporter *pr,
			FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TriSurfFieldToText %t %e.pts %e.tets";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextTriSurfField_plugin("TextTriSurfField",
			"", "",
			TextTriSurfField_reader,
			TextTriSurfField_writer);





static FieldHandle
TextPointCloudField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToPointCloudField %f %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextPointCloudField_writer(ProgressReporter *pr,
			   FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "PointCloudFieldToText %t %f";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextPointCloudField_plugin("TextPointCloudField",
			   "", "",
			   TextPointCloudField_reader,
			   TextPointCloudField_writer);



static FieldHandle
TextStructCurveField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToStructCurveField %f %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextStructCurveField_writer(ProgressReporter *pr,
			    FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "StructCurveFieldToText %t %f";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextStructCurveField_plugin("TextStructCurveField",
			    "", "",
			    TextStructCurveField_reader,
			    TextStructCurveField_writer);



static FieldHandle
TextStructHexVolField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToStructHexVolField %f %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextStructHexVolField_writer(ProgressReporter *pr,
			     FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "StructHexVolFieldToText %t %f";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextStructHexVolField_plugin("TextStructHexVolField",
			     "", "",
			     TextStructHexVolField_reader,
			     TextStructHexVolField_writer);


static FieldHandle
TextStructQuadSurfField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "TextToStructQuadSurfField %f %t -binOutput";
  return Exec_reader(pr, filename, command);
}

static bool
TextStructQuadSurfField_writer(ProgressReporter *pr,
			       FieldHandle field, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "StructQuadSurfFieldToText %t %f";
  return Exec_writer(pr, field, filename, command);
}

static FieldIEPlugin
TextStructQuadSurfField_plugin("TextStructQuadSurfField",
			       "", "",
			       TextStructQuadSurfField_reader,
			       TextStructQuadSurfField_writer);



static FieldHandle
VTKtoTriSurfField_reader(ProgressReporter *pr, const char *filename)
{
  const string command =
    string(SCIRUN_OBJDIR) + "/StandAlone/convert/" +
    "VTKtoTriSurfField %f %t -bin_out";
  return Exec_reader(pr, filename, command);
}

static FieldIEPlugin
VTKtoTriSurfField_plugin("VTKtoTriSurfField",
			 "", "",
			 VTKtoTriSurfField_reader,
			 NULL);
