/*
 * The MIT License
 *
 * Copyright (c) 2012 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "VardenParameters.h"

namespace Wasatch {


  VarDenParameters::VarDenParameters()
  {
    alpha0  = 0.1;
    model  = CONSTANT;
  };

  void parse_varden_input( Uintah::ProblemSpecP varDenSpec,
                          VarDenParameters& varDenInputParams )
  {
    if (!varDenSpec) return;
    
    // get the name of the turbulence model
    std::string varDenModelName;
    varDenSpec->getAttribute("model",varDenModelName);
    
    if ( varDenModelName.compare("CONSTANT") == 0   ) {
      varDenInputParams.model = VarDenParameters::CONSTANT;
    } else if ( varDenModelName.compare("IMPULSE") == 0 ) {
      varDenInputParams.model = VarDenParameters::IMPULSE;
    } else if ( varDenModelName.compare("DYNAMIC") == 0 ) {
      varDenInputParams.model = VarDenParameters::DYNAMIC;
    }
    // get the turbulent Prandtl number
    varDenSpec->getAttribute("coefficient",varDenInputParams.alpha0 );
  }
  
}
