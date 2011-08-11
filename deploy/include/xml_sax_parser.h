#ifndef __XML_SAX_PARSER_H__
#define __XML_SAX_PARSER_H__

#include "sax_parser_callbacks.h"

/*****************************************************
 * parse_xml - Parse the given xml for each callback.*
 *             This function is thread safe.         *
 * ***************************************************/
extern void parse_xml(parser_cbs cbs, char* xml);
/*******************************************************************
 * parse_xml_string - Parse the given xml string for each callback.*
 *             This function is thread safe.                       *
 * *****************************************************************/
extern void parse_xml_string(parser_cbs cbs, char* xml);
#endif
