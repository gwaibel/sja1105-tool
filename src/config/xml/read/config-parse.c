/******************************************************************************
 * Copyright (c) 2016, NXP Semiconductors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include "internal.h"

#ifndef LIBXML_TREE_ENABLED

int sja1105_config_read_from_xml(const char *xml_file, struct sja1105_config *config)
{
	loge("Tree support is not compiled in libxml2!");
	return -1;
}

#else

int xml_read_field(void *where, char *field_name, xmlNode *node)
{
	uint64_t *field_val;
	char     *value = NULL;
	int       rc = 0;
	xmlNode  *cur;

	field_val = (uint64_t*) where;
	for (cur = node->children; cur != NULL; cur = cur->next) {
		if (xmlStrcmp(cur->name, (const xmlChar*) field_name) == 0) {
			value = (char*) xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
		}
	}
	if (value == NULL) {
		loge("no element named \"%s\"!", field_name);
		rc = -1;
		goto out;
	}
	rc = reliable_uint64_from_string(field_val, value, NULL);
out:
	xmlFree(value);
	return rc;
}

int xml_read_array(void *where, int max_count, char *field_name, xmlNode *node)
{
	uint64_t *field_val;
	char     *value = NULL;
	int       rc = 0;
	xmlNode  *cur;

	/* Convert "where" to an array of uint64_t */
	field_val = (uint64_t*) where;
	/* Get the "field_name" property into our "value" string */
	for (cur = node->children; cur != NULL; cur = cur->next) {
		if (xmlStrcmp(cur->name, (const xmlChar*) field_name) == 0) {
			value = (char*) xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
		}
	}
	if (value == NULL) {
		loge("no element named \"%s\"!", field_name);
		rc = -1;
		goto out;
	}
	rc = read_array(value, field_val, max_count);
out:
	xmlFree(value);
	return rc;
}

static int parse_config_table(xmlNode *node, struct sja1105_config *config)
{
	char *table_name;

	const char *options[] = {
		"schedule-table",
		"schedule-entry-points-table",
		"vl-lookup-table",
		"vl-policing-table",
		"vl-forwarding-table",
		"l2-address-lookup-table",
		"l2-policing-table",
		"vlan-lookup-table",
		"l2-forwarding-table",
		"mac-configuration-table",
		"schedule-parameters-table",
		"schedule-entry-points-parameters-table",
		"vl-forwarding-parameters-table",
		"l2-address-lookup-parameters-table",
		"l2-forwarding-parameters-table",
		"clock-synchronization-parameters-table",
		"avb-parameters-table",
		"general-parameters-table",
		"retagging-table",
		"xmii-mode-parameters-table",
	};
	int (*next_parse_config_table[])(xmlNode *, struct sja1105_config *) = {
		schedule_table_parse,
		schedule_entry_points_table_parse,
		vl_lookup_table_parse,
		vl_policing_table_parse,
		vl_forwarding_table_parse,
		l2_address_lookup_table_parse,
		l2_policing_table_parse,
		vlan_lookup_table_parse,
		l2_forwarding_table_parse,
		mac_configuration_table_parse,
		schedule_parameters_table_parse,
		schedule_entry_points_parameters_table_parse,
		vl_fw_params_table_parse,
		l2_address_lookup_parameters_table_parse,
		l2_forwarding_parameters_table_parse,
		clock_synchronization_parameters_table_parse,
		avb_parameters_table_parse,
		general_parameters_table_parse,
		retagging_table_parse,
		xmii_mode_parameters_table_parse,
	};
	int rc;
	table_name = (char*) node->name;
	rc = get_match(table_name, options, ARRAY_SIZE(options));
	if (rc < 0) {
		goto out;
	}
	rc = next_parse_config_table[rc](node, config);
out:
	return rc;
}

static int parse_root(xmlNode *root, struct sja1105_config *config)
{
	xmlNode *node;
	int rc = 0;

	if (root->type != XML_ELEMENT_NODE) {
		loge("Root node must be of element type!");
		rc = -1;
		goto out;
	}
	if (strcasecmp((char*) root->name, SJA1105_NETCONF_ROOT)) {
		loge("Root node must be named \"%s\"!", SJA1105_NETCONF_ROOT);
		rc = -1;
		goto out;
	}
	for (node = root->children; node != NULL; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		rc = parse_config_table(node, config);
		if (rc < 0) {
			loge("Could not parse XML file!");
			goto out;
		}
	}
out:
	return rc;
}

int sja1105_config_read_from_xml(const char *xml_file, struct sja1105_config *config)
{
	xmlNode *root = NULL;
	xmlDoc  *doc = NULL;
	int      rc = 0;

	/*
	 * this initializes the library and checks potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION;

	doc = xmlReadFile(xml_file, NULL, 0);
	if (doc == NULL) {
		loge("could not parse file %s", xml_file);
	}
	root = xmlDocGetRootElement(doc);
	if (!root) {
		loge("failed to get root element");
		goto out;
	}
	memset(config, 0, sizeof(*config));
	rc = parse_root(root, config);
out:
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return rc;
}

#endif
