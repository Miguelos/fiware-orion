/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/
#include <stdio.h>
#include <string>

#include "logMsg/logMsg.h"
#include "logMsg/traceLevels.h"

#include "common/globals.h"
#include "common/limits.h"
#include "common/tag.h"
#include "common/string.h"
#include "alarmMgr/alarmMgr.h"

#include "orionTypes/OrionValueType.h"
#include "parse/forbiddenChars.h"
#include "ngsi/Metadata.h"

#include "mongoBackend/dbConstants.h"
#include "mongoBackend/safeMongo.h"
#include "mongoBackend/compoundResponses.h"

#include "rest/ConnectionInfo.h"

using namespace mongo;



/* ****************************************************************************
*
* Metadata::~Metadata -
*/
Metadata::~Metadata()
{
  release();
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata()
{
  name            = "";
  type            = "";
  stringValue     = "";
  valueType       = orion::ValueTypeString;
  typeGiven       = false;
  compoundValueP  = NULL;
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(Metadata* mP, bool useDefaultType)
{
  LM_T(LmtClone, ("'cloning' a Metadata"));

  name            = mP->name;
  type            = mP->type;
  valueType       = mP->valueType;
  stringValue     = mP->stringValue;
  numberValue     = mP->numberValue;
  boolValue       = mP->boolValue;
  typeGiven       = mP->typeGiven;
  compoundValueP  = (mP->compoundValueP != NULL)? mP->compoundValueP->clone() : NULL;

  if (useDefaultType && !typeGiven)
  {
    if ((compoundValueP == NULL) || (compoundValueP->valueType != orion::ValueTypeVector))
    {
      type = defaultType(valueType);
    }
    else
    {
      type = defaultType(orion::ValueTypeVector);
    }
  }
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(const std::string& _name, const std::string& _type, const char* _value)
{
  name            = _name;
  type            = _type;
  valueType       = orion::ValueTypeString;
  stringValue     = std::string(_value);
  typeGiven       = false;
  compoundValueP  = NULL;
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(const std::string& _name, const std::string& _type, const std::string& _value)
{
  name            = _name;
  type            = _type;
  valueType       = orion::ValueTypeString;
  stringValue     = _value;
  typeGiven       = false;
  compoundValueP  = NULL;
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(const std::string& _name, const std::string& _type, double _value)
{
  name            = _name;
  type            = _type;
  valueType       = orion::ValueTypeNumber;
  numberValue     = _value;
  typeGiven       = false;
  compoundValueP  = NULL;
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(const std::string& _name, const std::string& _type, bool _value)
{
  name            = _name;
  type            = _type;
  valueType       = orion::ValueTypeBoolean;
  boolValue       = _value;
  typeGiven       = false;
  compoundValueP  = NULL;
}



/* ****************************************************************************
*
* Metadata::Metadata -
*/
Metadata::Metadata(const std::string& _name, const BSONObj& mdB)
{
  name            = _name;
  type            = mdB.hasField(ENT_ATTRS_MD_TYPE) ? getStringFieldF(mdB, ENT_ATTRS_MD_TYPE) : "";
  typeGiven       = (type == "")? false : true;
  compoundValueP  = NULL;

  BSONType bsonType = getFieldF(mdB, ENT_ATTRS_MD_VALUE).type();
  switch (bsonType)
  {
  case String:
    valueType   = orion::ValueTypeString;
    stringValue = getStringFieldF(mdB, ENT_ATTRS_MD_VALUE);
    break;

  case NumberDouble:
    valueType   = orion::ValueTypeNumber;
    numberValue = getFieldF(mdB, ENT_ATTRS_MD_VALUE).Number();
    break;

  case Bool:
    valueType = orion::ValueTypeBoolean;
    boolValue = getBoolFieldF(mdB, ENT_ATTRS_MD_VALUE);
    break;

  case jstNULL:
    valueType = orion::ValueTypeNone;
    break;

  case Object:
  case Array:
    valueType      = orion::ValueTypeObject;
    compoundValueP = new orion::CompoundValueNode();
    compoundObjectResponse(compoundValueP, getFieldF(mdB, ENT_ATTRS_VALUE));
    compoundValueP->container = compoundValueP;
    compoundValueP->name      = "value";
    compoundValueP->valueType = (bsonType == Object)? orion::ValueTypeObject : orion::ValueTypeVector;
    break;

  default:
    valueType = orion::ValueTypeUnknown;
    LM_E(("Runtime Error (unknown metadata value value type in DB: %d)", getFieldF(mdB, ENT_ATTRS_MD_VALUE).type()));
    break;
  }
}



/* ****************************************************************************
*
* Metadata::render -
*/
std::string Metadata::render(const std::string& indent, bool comma)
{
  std::string out     = "";
  std::string tag     = "contextMetadata";
  std::string xValue  = toStringValue();

  out += startTag2(indent, tag, false, false);
  out += valueTag1(indent + "  ", "name", name, true);
  out += valueTag1(indent + "  ", "type", type, true);

  if (valueType == orion::ValueTypeString)
  {
    out += valueTag1(indent + "  ", "value", xValue, false);
  }
  else if (valueType == orion::ValueTypeNumber)
  {
    out += indent + "  " + JSON_STR("value") + ": " + xValue;
  }
  else if (valueType == orion::ValueTypeBoolean)
  {
    out += indent + "  " + JSON_STR("value") + ": " + xValue;
  }
  else if (valueType == orion::ValueTypeNone)
  {
    out += indent + "  " + JSON_STR("value") + ": " + xValue; 
  }
  else if (valueType == orion::ValueTypeObject)
  {
    std::string part;

    part = compoundValueP->toJson(true, false);
    out += part;
  }
  else
  {
    out += indent + "  " + JSON_STR("value") + ": " + JSON_STR("unknown json type");
  }

  out += endTag(indent, comma);

  return out;
}



/* ****************************************************************************
*
* Metadata::check -
*/
std::string Metadata::check
(
  ConnectionInfo*     ciP,
  RequestType         requestType,
  const std::string&  indent,
  const std::string&  predetectedError,
  int                 counter
)
{
  size_t len;
  char   errorMsg[128];

  if (name == "")
  {
    alarmMgr.badInput(clientIp, "missing metadata name");
    return "missing metadata name";
  }

  if ( (len = strlen(name.c_str())) > MAX_ID_LEN)
  {
    snprintf(errorMsg, sizeof errorMsg, "metadata name length: %zd, max length supported: %d", len, MAX_ID_LEN);
    alarmMgr.badInput(clientIp, errorMsg);
    return std::string(errorMsg);
  }

  if (forbiddenIdChars(ciP->apiVersion , name.c_str()))
  {
    alarmMgr.badInput(clientIp, "found a forbidden character in the name of a Metadata");
    return "Invalid characters in metadata name";
  }

  if ( (len = strlen(type.c_str())) > MAX_ID_LEN)
  {
    snprintf(errorMsg, sizeof errorMsg, "metadata type length: %zd, max length supported: %d", len, MAX_ID_LEN);
    alarmMgr.badInput(clientIp, errorMsg);
    return std::string(errorMsg);
  }


  if (ciP->apiVersion == "v2" && (len = strlen(type.c_str())) < MIN_ID_LEN)
  {
    snprintf(errorMsg, sizeof errorMsg, "metadata type length: %zd, min length supported: %d", len, MIN_ID_LEN);
    alarmMgr.badInput(clientIp, errorMsg);
    return std::string(errorMsg);
  }

  if (forbiddenIdChars(ciP->apiVersion, type.c_str()))
  {
    alarmMgr.badInput(clientIp, "found a forbidden character in the type of a Metadata");
    return "Invalid characters in metadata type";
  }

  if (valueType == orion::ValueTypeString)
  {
    if (forbiddenChars(stringValue.c_str()))
    {
      alarmMgr.badInput(clientIp, "found a forbidden character in the value of a Metadata");
      return "Invalid characters in metadata value";
    }

    if (stringValue == "")
    {
      alarmMgr.badInput(clientIp, "missing metadata value");
      return "missing metadata value";
    }
  }

  return "OK";
}



/* ****************************************************************************
*
* Metadata::present -
*/
void Metadata::present(const std::string& metadataType, int ix, const std::string& indent)
{
  LM_T(LmtPresent, ("%s%s Metadata %d:",   
		    indent.c_str(), 
		    metadataType.c_str(), 
		    ix));
  LM_T(LmtPresent, ("%s  Name:     %s", 
		    indent.c_str(), 
		    name.c_str()));
  LM_T(LmtPresent, ("%s  Type:     %s", 
		    indent.c_str(), 
		    type.c_str()));
  LM_T(LmtPresent, ("%s  Value:    %s", 
		    indent.c_str(), 
		    stringValue.c_str()));
}



/* ****************************************************************************
*
* release -
*/
void Metadata::release(void)
{
  if (compoundValueP != NULL)
  {
    delete compoundValueP;
    compoundValueP = NULL;
  }
}



/* ****************************************************************************
*
* fill - 
*/
void Metadata::fill(const struct Metadata& md)
{
  name         = md.name;
  type         = md.type;
  stringValue  = md.stringValue;
}



/* ****************************************************************************
*
* toStringValue -
*/
std::string Metadata::toStringValue(void) const
{
  char buffer[64];

  switch (valueType)
  {
  case orion::ValueTypeString:
    return stringValue;
    break;

  case orion::ValueTypeNumber:
    snprintf(buffer, sizeof(buffer), "%f", numberValue);
    return std::string(buffer);
    break;

  case orion::ValueTypeBoolean:
    return boolValue ? "true" : "false";
    break;

  case orion::ValueTypeNone:
    return "null";
    break;

  default:
    return "<unknown type>";
    break;
  }

  // Added to avoid warning when compiling with -fstack-check -fstack-protector
  return "";
}



/* ****************************************************************************
*
* toJson - 
*/
std::string Metadata::toJson(bool isLastElement)
{
  std::string  out;

  out = JSON_STR(name) + ":{";

  /* This is needed for entities coming from NGSIv1 (which allows empty or missing types) */
  std::string defType = defaultType(valueType);

  if (compoundValueP && compoundValueP->isVector())
  {
    defType = defaultType(orion::ValueTypeVector);
  }

  out += (type != "")? JSON_VALUE("type", type) : JSON_VALUE("type", defType);
  out += ",";

  if (valueType == orion::ValueTypeString)
  {
    out += JSON_VALUE("value", stringValue);
  }
  else if (valueType == orion::ValueTypeNumber)
  {
    out += JSON_VALUE_NUMBER("value", toString(numberValue));
  }
  else if (valueType == orion::ValueTypeBoolean)
  {
    out += JSON_VALUE_BOOL("value", boolValue);
  }
  else if (valueType == orion::ValueTypeNone)
  {
    out += JSON_STR("value") + ":null";
  }
  else if ((valueType == orion::ValueTypeObject) || (valueType == orion::ValueTypeVector))
  {
    if ((compoundValueP->isObject()) || (compoundValueP->isVector()))
    {
      std::string out2;

      //
      // FIXME P1
      //   These two 'funny' lines, modifying the compound, pretending it is not
      //   toplevel, and setting its name to 'value' is to make toJson() work correctly
      //   for metadata.
      //
      //   The toJson method must work both for attributes and metadata.
      //   Attributes can be rendered with 'keyValues=on', and that special case we
      //   don't want for metadata.
      //
      //   This 'hack' was the easiest way I could find to make the rendering of compounds
      //   for metadata work - might not be the optimal way. A bool parameter could be passed, for example.
      //
      
      compoundValueP->name       = "value";
      compoundValueP->rootP      = NULL;

      LM_W(("KZ: Setting renderName to TRUE"));
      compoundValueP->renderName = true;

      std::string r = compoundValueP->toJson(isLastElement, false);
      LM_W(("KZ: rendered compoundValue: %s", r.c_str()));
      out += r;
    }
  }
  else
  {
    LM_E(("Runtime Error (invalid value type for metadata %s)", name.c_str()));
    out += JSON_VALUE("value", stringValue);
  }

  out += "}";

  if (!isLastElement)
  {
    out += ",";
  }

  return out;
}



/* ****************************************************************************
*
* Metadata::compoundItemExists - 
*/
bool Metadata::compoundItemExists(const std::string& compoundPath, orion::CompoundValueNode** compoundItemPP)
{
  std::vector<std::string>   compoundPathV;
  orion::CompoundValueNode*  current = compoundValueP;
  int                        levels;

  if (compoundPath == "")
  {
    return false;
  }

  if (compoundValueP == NULL)
  {
    return false;
  }

  levels = stringSplit(compoundPath, '.', compoundPathV);

  if ((compoundPathV.size() == 0) || (levels == 0))
  {
    return false;
  }

  for (int ix = 0; ix < levels; ++ix)
  {
    bool found = false;

    for (unsigned int cIx = 0; cIx < current->childV.size(); ++cIx)
    {
      if (current->childV[cIx]->name == compoundPathV[ix])
      {
        current = current->childV[cIx];
        found   = true;
        break;
      }
    }

    if (found == false)
    {
      return false;
    }
  }

  if (compoundItemPP != NULL)
  {
    *compoundItemPP = current;
  }

  return true;
}
