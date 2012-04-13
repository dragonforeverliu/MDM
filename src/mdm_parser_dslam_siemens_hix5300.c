/*!
 * \file mdm_parser_dslam_siemens_hix5300.c Parsers for dslams siemens hix5300.
 *
 * \author Marcelo Gornstein <marcelog@gmail.com>
 */
#include    <stdio.h>
#include    <stdlib.h>
#include    <unistd.h>
#include    <string.h>
#include    <libxml/parser.h>
#include    <libxml/tree.h>
#include    <mdm_parser_dslam_siemens_hix5300.h>
#include    <mdm.h>

/*******************************************************************************
 * CODE STARTS.
 ******************************************************************************/
/**
 * Dummy.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_nop(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    d->exec_buffer_post_len = 0;
    return;
}

typedef struct _section_struct
{
    char *start;
    int length;
    struct _section_struct *next;
} section_t;

static section_t *
dslam_siemens_hix5300_section_alloc()
{
    section_t *section = (section_t *)MDM_MALLOC(sizeof(section_t));
    return section;
}

static section_t *
dslam_siemens_hix5300_section_create(const char *start, int length)
{
    section_t *section = dslam_siemens_hix5300_section_alloc();
    section->start = (char *)MDM_MALLOC(length + 1);
    memcpy(section->start, start, length);
    section->length = length;
    section->next = NULL;
    return section;
}
static void
dslam_siemens_hix5300_section_free(section_t *section)
{
    if (section->next != NULL) {
        dslam_siemens_hix5300_section_free(section->next);
    }
    MDM_MFREE(&section->start);
    MDM_MFREE(&section);
}

static section_t *
dslam_siemens_hix5300_section_add(section_t *addTo, const char *start, int length)
{
    addTo->next = dslam_siemens_hix5300_section_create(start, length);
    return addTo->next;
}

static void
dslam_siemens_hix5300_parse_with_slash(
    const char *subject, char *value1, int maxLength1, char *value2, int maxLength2
) {
    const char *slash = strchr(subject, '/');
    const char *start;
    const char *end;
    const char *maxSubjectLength = subject + strlen(subject);
    int length = 0;
    if (slash == NULL)
    {
        *value1 = 0;
        *value2 = 0;
        return;
    }
    start = subject;
    while(*start == 32) start++;
    end = slash;
    length = end - start;
    if (length > maxLength1)
    {
        length = maxLength1 - 1;
    }
    snprintf(value1, length + 1, "%s", start);
    end++;
    start = end;
    while(*start == 32) start++;
    end = start;
    while(*end != 32 && *end != 13 && end < maxSubjectLength) end++;
    length = end - start;
    if (length > maxLength2)
    {
        length = maxLength2 - 1;
    }
    snprintf(value2, length + 1, "%s", start);
}

static void
dslam_siemens_hix5300_parse_with_spaces(
    const char *subject, const char *key, char *value, int maxLength
) {
    const char *needle;
    const char *eol;
    int length;
    needle = strstr(subject, key);
    if (needle == NULL) {
        *value = 0;
        return;
    }
    needle += strlen(key);
    while(*needle == 32) needle++;
    while(*needle == ':') needle++;
    while(*needle == 32) needle++;
    eol = strchr(needle, 13);
    if (eol == NULL) {
        length = strlen(needle);
    } else {
        length = eol - needle;
    }
    if (length > maxLength) {
        length = maxLength - 1;
    }
    snprintf(value, length + 1, "%s", needle);
}

static section_t *
dslam_siemens_hix5300_parse_lines(const char *subject)
{
    const char *start;
    const char *end;
    const char *absoluteEnd = subject + strlen(subject);
    int length;
    section_t *section = NULL;
    section_t *parentSection = NULL;
    start = subject;
    do
    {
        end = strchr(start, 13);
        if (end == NULL) {
            end = absoluteEnd;
        }
        length = end - start;
        if (section != NULL) {
            section = dslam_siemens_hix5300_section_add(section, start, length);
        } else {
            section = dslam_siemens_hix5300_section_create(start, length);
            parentSection = section;
        }
        start = end + 2;
    } while(start < absoluteEnd);
    return parentSection;
}

static section_t *
dslam_siemens_hix5300_parse_section(const char *subject)
{
    const char *start;
    const char *end;
    const char *eol;
    const char *absoluteEnd = subject + strlen(subject);
    int length;
    section_t *section = NULL;
    section_t *parentSection = NULL;
    start = subject;
    do
    {
        start = strstr(start, "--------");
        if (start == NULL) {
            break;
        }
        eol = strchr(start, 13);
        if (eol == NULL) {
            break;
        }
        start = eol + 2;

        end = strstr(start, "--------");
        if (end == NULL) {
            end = strstr(start, "========");
            if (end == NULL) {
                end = absoluteEnd;
            }
        }
        length = end - start;
        if (section != NULL) {
            section = dslam_siemens_hix5300_section_add(section, start, length);
        } else {
            section = dslam_siemens_hix5300_section_create(start, length);
            parentSection = section;
        }
        start = end;
    } while(start != NULL);
    return parentSection;
}

static int
dslam_siemens_hix5300_xml_alloc(
    xmlDocPtr *doc, xmlNodePtr *root_node, xmlBufferPtr *psBuf,
    char *rootName, mdm_operation_result_t *status
) {
    /* Create target buffer. */
    *psBuf = xmlBufferCreate();
    if(*psBuf == NULL)
    {
        status->status = MDM_OP_ERROR;
        sprintf(status->status_message, "Error creating buffer for xml.");
        return -1;
    }

    /* Creates a new document, a node and set it as a root node */
    *doc = xmlNewDoc(BAD_CAST "1.0");
    if(*doc == NULL)
    {
        status->status = MDM_OP_ERROR;
        sprintf(status->status_message, "Error creating doc xml.");
        return -1;
    }

    *root_node = xmlNewNode(NULL, BAD_CAST rootName);
    if(*root_node == NULL)
    {
        status->status = MDM_OP_ERROR;
        sprintf(status->status_message, "Error creating doc xml.");
        return -1;
    }
    xmlDocSetRootElement(*doc, *root_node);
    return 0;
}

static void
dslam_siemens_hix5300_xml_free(xmlDocPtr *doc, xmlBufferPtr *psBuf)
{
    if(*doc != NULL)
        xmlFreeDoc(*doc);
    if(*psBuf != NULL)
        xmlBufferFree(*psBuf);
}

static void
dslam_siemens_hix5300_xml_add(xmlNodePtr node, char *nodeName, char *value)
{
    xmlNewChild(node, NULL, BAD_CAST nodeName, BAD_CAST value);
}

static void
dslam_siemens_hix5300_xml_add_from_section(
    xmlNodePtr node, char *token, char *nodeName, section_t *section
) {
    char buffer[4096];
    dslam_siemens_hix5300_parse_with_spaces(
        section->start, token, buffer, sizeof(buffer)
    );
    dslam_siemens_hix5300_xml_add(node, nodeName, buffer);
}

static void
dslam_siemens_hix5300_parse_speed(
    const char *what, const char *line, char *buffer, int maxLength
) {
    char needle[4096];
    const char *start;
    const char *end;
    int length;
    snprintf(needle, sizeof(needle), "%s[", what);
    start = strstr(line, needle);
    if (start == NULL) {
        *buffer = 0;
        return;
    }
    start += strlen(needle);
    while(*start == 32) start++;
    end = strchr(start, ']');
    if (end == NULL) {
        *buffer = 0;
        return;
    }
    length = end - start;
    if (length > maxLength) {
        length = maxLength - 1;
    }
    snprintf(buffer, length + 1, "%s", start);
}

static void
dslam_siemens_hix5300_parse_speed_up(const char *line, char *buffer, int maxLength)
{
    dslam_siemens_hix5300_parse_speed("UP ", line, buffer, maxLength);
}

static void
dslam_siemens_hix5300_parse_speed_down(const char *line, char *buffer, int maxLength)
{
    dslam_siemens_hix5300_parse_speed("DOWN ", line, buffer, maxLength);
}

static const char *
dslam_siemens_hix5300_get_word_delimited_by(
    const char *subject, int subjectLen, char token, char *buffer, int maxLength
) {
    const char *start;
    const char *end;
    const char *absoluteEnd = subject + subjectLen;
    int length;

    while(*subject == token) subject++;
    start = subject;
    while(*subject != token && *subject != 13 && subject < absoluteEnd) subject++;
    end = subject;
    length = end - start;
    if (length > maxLength)
    {
        length = maxLength - 1;
    }
    snprintf(buffer, length + 1, "%s", start);
    return end;
}
/*
OS1:     she-r15-cxu_b-o.1205
                length=9736192 Bytes ->Default OS ->Trial OS, used for reboot
OS2:     she-r15-cxu_b-o.1196
                length=9719808 Bytes
running: she-r15-cxu_b-o.1205 # build=#1 Thu Mar 22 11:07:04 CST 2007
(running is OS1)
*/

/*!
 * This will try to get software versions.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_soft_versions(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[128];
    const char *start;
    const char *end;

    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_soft_versions", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_soft_versions_done;
    }

    start = strstr(d->exec_buffer_post, "OS1:");
    if (start != NULL) {
        start += strlen("OS1:") + 1;
        while(*start == 32) start++;
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "os1-version", buffer);
        start = end + 2;
        while(*start == 32 || *start == 9) start++;
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "os1-description", buffer);
    }

    start = strstr(d->exec_buffer_post, "OS2:");
    if (start != NULL) {
        start += strlen("OS2:") + 1;
        while(*start == 32) start++;
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "os2-version", buffer);
        start = end + 2;
        while(*start == 32 || *start == 9) start++;
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "os2-description", buffer);
    }

    start = strstr(d->exec_buffer_post, "running: ");
    if (start != NULL) {
        start += strlen("running: ");
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "running-version", buffer);
    }

    if (strstr(d->exec_buffer_post, "(running is OS1)") != NULL) {
        dslam_siemens_hix5300_xml_add(root_node, "running-slot", "1");
    } else {
        dslam_siemens_hix5300_xml_add(root_node, "running-slot", "2");
    }
    xmlNodeDump(psBuf, doc, root_node, 99, 1);

    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_soft_versions_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get alarms.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_alarms(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    char *start;
    char *end;
    start = strstr(d->exec_buffer, "Alarm List :");
    if (start == NULL) {
        snprintf(
            d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
            "<siemens_hix5300_alarms/>"
        );
        d->exec_buffer_post_len = strlen(d->exec_buffer);
        return;
    }
    start = strchr(start, 13) + 2;
    end = strstr(start, "All values are decimals");
    if (end == NULL) {
        end = start + d->exec_buffer_len;
    }
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "<siemens_hix5300_alarms><![CDATA[%.*s]]></siemens_hix5300_alarms>",
        (int)(end - start), start
    );
    d->exec_buffer_post_len = strlen(d->exec_buffer_post);
    return;
}

/*!
 * This will try to get ntp information.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_ntp(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[128];
    const char *start;
    const char *end;

    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_ntp", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_ntp_done;
    }

    start = strstr(d->exec_buffer_post, "ntp-client server ");
    if (start != NULL) {
        start += strlen("ntp-client server ");
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "server", buffer);
    }

    start = strstr(d->exec_buffer_post, "ntp-client interval ");
    if (start != NULL) {
        start += strlen("ntp-client interval ");
        end = strchr(start, 13);
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(root_node, "interval", buffer);
    }

    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_ntp_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get system version
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_system_version(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[128];

    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_version", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_system_version_done;
    }

    dslam_siemens_hix5300_parse_with_spaces(
        d->exec_buffer_post, "System version",
        buffer, sizeof(buffer)
    );
    dslam_siemens_hix5300_xml_add(root_node, "version", buffer);
    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_system_version_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get line config.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_line_config(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlNodePtr node = NULL;
    xmlBufferPtr psBuf = NULL;
    char *lineStart;
    char *lineEnd;
    char buffer[128];
    section_t *sections = dslam_siemens_hix5300_parse_section(d->exec_buffer_post);
    section_t *currentSection = NULL;
    section_t *currentLine = NULL;
    section_t *lines = NULL;
    const char *valueStart;
    const char *valueEnd;
    int i = 0;
    char *tokens[] = {
        "slot", "port", "admin", "mode", "type", "speed-up", "speed-down",
        "snr-up", "snr-down", "interleave-delay-up", "interleave-delay-down",
        NULL
    };
    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_line_config", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_line_config_done;
    }
    if (sections != NULL) {
        currentSection = sections;
        if (currentSection->next != NULL) {
            currentSection = currentSection->next;
        }
    }
    if (currentSection != NULL)
    {
        lineStart = currentSection->start;
        lines = dslam_siemens_hix5300_parse_lines(lineStart);
        if (lines != NULL) {
            currentLine = lines;
            do
            {
                node = xmlNewNode(NULL, BAD_CAST "line");
                lineStart = currentLine->start;
                lineEnd = lineStart + currentLine->length;
                i = 0;
                valueStart = lineStart;
                while(tokens[i] != NULL) {
                    while(*valueStart == 32 || *valueStart == '/') valueStart++;
                    valueEnd = valueStart;
                    while(*valueEnd != 32 && *valueEnd != '/') valueEnd++;
                    snprintf(buffer, valueEnd - valueStart + 1, "%s", valueStart);
                    dslam_siemens_hix5300_xml_add(node, tokens[i], buffer);
                    i++;
                    valueStart = valueEnd;
                }
                xmlAddChild(root_node, node);
                currentLine = currentLine->next;
            } while(currentLine != NULL);
        }
    }
    dslam_siemens_hix5300_section_free(sections);
    if (lines != NULL) {
        dslam_siemens_hix5300_section_free(lines);
    }

    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_line_config_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get all slot ports.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_slot_ports(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlNodePtr node = NULL;
    xmlBufferPtr psBuf = NULL;
    char *lineStart;
    char *lineEnd;
    char buffer[128];
    section_t *sections = dslam_siemens_hix5300_parse_section(d->exec_buffer_post);
    section_t *currentSection = NULL;
    section_t *currentLine = NULL;
    section_t *lines = NULL;
    const char *valueStart;
    const char *valueEnd;
    int i = 0;
    char *tokens[] = {
        "slot", "port", "admin", "oper", "speed-up", "speed-down",
        "snr-up", "snr-down", "attenuation-up", "attenuation-down",
        "tx-up", "tx-down", "attainable-up", "attainable-down", NULL
    };
    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_slotports", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_slot_ports_done;
    }
    if (sections != NULL) {
        currentSection = sections;
        if (currentSection->next != NULL) {
            currentSection = currentSection->next;
        }
    }
    if (currentSection != NULL)
    {
        lineStart = currentSection->start;
        lines = dslam_siemens_hix5300_parse_lines(lineStart);
        if (lines != NULL) {
            currentLine = lines;
            do
            {
                node = xmlNewNode(NULL, BAD_CAST "port");
                lineStart = currentLine->start;
                lineEnd = lineStart + currentLine->length;
                i = 0;
                valueStart = lineStart;
                while(tokens[i] != NULL) {
                    while(*valueStart == 32 || *valueStart == '/') valueStart++;
                    valueEnd = valueStart;
                    while(*valueEnd != 32 && *valueEnd != '/') valueEnd++;
                    snprintf(buffer, valueEnd - valueStart + 1, "%s", valueStart);
                    dslam_siemens_hix5300_xml_add(node, tokens[i], buffer);
                    i++;
                    valueStart = valueEnd;
                }
                xmlAddChild(root_node, node);
                currentLine = currentLine->next;
            } while(currentLine != NULL);
        }
    }
    dslam_siemens_hix5300_section_free(sections);
    if (lines != NULL) {
        dslam_siemens_hix5300_section_free(lines);
    }

    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_slot_ports_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get all slots.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_slots(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlNodePtr node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[128];
    const char *start;

    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_slotlist", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_slots_done;
    }

    start = strchr(d->exec_buffer_post, 13);
    start += 2;
    while((start = strchr(start, 13)) != NULL) {
        node = xmlNewNode(NULL, BAD_CAST "slot");
        start += 2;
        start = dslam_siemens_hix5300_get_word_delimited_by(
            start, strlen(start), 32, buffer, sizeof(buffer)
        );
        dslam_siemens_hix5300_xml_add(node, "id", buffer);

        start = dslam_siemens_hix5300_get_word_delimited_by(
            start, strlen(start), 32, buffer, sizeof(buffer)
        );
        dslam_siemens_hix5300_xml_add(node, "uport", buffer);
        start = dslam_siemens_hix5300_get_word_delimited_by(
            start, strlen(start), 32, buffer, sizeof(buffer)
        );
        dslam_siemens_hix5300_xml_add(node, "tx-pause", buffer);
        start = dslam_siemens_hix5300_get_word_delimited_by(
            start, strlen(start), 32, buffer, sizeof(buffer)
        );
        dslam_siemens_hix5300_xml_add(node, "rx-pause", buffer);
        xmlAddChild(root_node, node);
    }

    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_slots_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get physical port information.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_physical_port_info(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlNodePtr node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[512];
    const char *start;
    const char *end;
    const char *tmpend;
    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_physical_ports", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_physical_port_info_done;
    }
    start = d->exec_buffer_post;
    while((start = strstr(start, "Port ")) != NULL)
    {
        node = xmlNewNode(NULL, BAD_CAST "physicalport");

        // slot
        start += 5;
        end = strchr(start, '/');
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "slot", buffer);
        end++;

        start = end;
        while(*end != 13 && *end != 32) end++;
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "port", buffer);

        start = strstr(start, "Line Profile Name") + strlen("Line Profile Name");
        start = strchr(start, ':') + 1;
        while(*start == 32) start++;
        end = start;
        while(*end != 13 && *end != 32) end++;
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "line-profile", buffer);

        start = strstr(start, "Alarm Profile Name") + strlen("Alarm Profile Name");
        start = strchr(start, ':') + 1;
        while(*start == 32) start++;
        end = start;
        while(*end != 13 && *end != 32) end++;
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "alarm-profile", buffer);

        start = strstr(start, "LineType") + strlen("LineType");
        start = strchr(start, ':') + 1;
        while(*start == 32) start++;
        end = start;
        while(*end != 13 && *end != 32 && *end != ',') end++;
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "line-type", buffer);

        start = strstr(start, "ATUC Capability") + strlen("ATUC Capability");
        start = strchr(start, 13) + 2;
        end = strstr(start, "ATUC Capability");
        memset(buffer, 0, sizeof(buffer));
        while(start < end)
        {
            while(*start == 32 || *start == '-') start++;
            tmpend = strchr(start, 13);
            strncat(buffer, start, tmpend - start);
            strcat(buffer, " ");
            start = tmpend + 2;
        }
        dslam_siemens_hix5300_xml_add(node, "atuc-capability", buffer);
        start = end;

        start = strstr(start, "ATUR Capability") + strlen("ATUR Capability");
        start = strchr(start, 13) + 2;
        end = strstr(start, "ATUR Capability");
        memset(buffer, 0, sizeof(buffer));
        while(start < end)
        {
            while(*start == 32 || *start == '-') start++;
            tmpend = strchr(start, 13);
            strncat(buffer, start, tmpend - start);
            strcat(buffer, " ");
            start = tmpend + 2;
        }
        dslam_siemens_hix5300_xml_add(node, "atur-capability", buffer);
        start = end;

        start = strstr(start, "ATUC ActualCapability") + strlen("ATUC ActualCapability");
        start = strchr(start, ':') + 1;
        while(*start == 32) start++;
        end = start;
        while(*end != 13 && *end != 32) end++;
        snprintf(buffer, end - start + 1, "%s", start);
        dslam_siemens_hix5300_xml_add(node, "atuc-actualcapability", buffer);

        xmlAddChild(root_node, node);
    }
    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_physical_port_info_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

/*!
 * This will try to get all service profiles.
 * \param d Device descriptor.
 * \param status Result of the operation.
 */
void
dslam_siemens_hix5300_get_service_profile(
    mdm_device_descriptor_t *d, mdm_operation_result_t *status
)
{
    xmlDocPtr doc = NULL; /* document pointer */
    xmlNodePtr root_node = NULL;
    xmlNodePtr node = NULL;
    xmlBufferPtr psBuf = NULL;
    char buffer[128];
    char buffer2[128];

    if (dslam_siemens_hix5300_xml_alloc(
        &doc, &root_node, &psBuf, "siemens_hix5300_serviceprofilelist", status
    ) == -1) {
        goto dslam_siemens_hix5300_get_service_profile_done;
    }
    section_t *sections = dslam_siemens_hix5300_parse_section(d->exec_buffer_post);
    section_t *currentSection;
    if (sections != NULL) {
        currentSection = sections;
        do {
            node = xmlNewNode(NULL, BAD_CAST "serviceprofile");
            dslam_siemens_hix5300_xml_add_from_section(node, "Profile Name", "name", currentSection);
            dslam_siemens_hix5300_xml_add_from_section(node, "Ratemode", "ratemode", currentSection);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Target SNR Margin(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "target-snr-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "target-snr-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-C Max SNR Margind(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed("", buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-max-snr", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-C Min SNR Margind(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed("", buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-min-snr", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-R Max SNR Margind(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed("", buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-max-snr", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-R Min SNR Margind(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed("", buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-min-snr", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-C Shift SNR Margin(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-shift-snr-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-shift-snr-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-R Shift SNR Margin(dB)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-shift-snr-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-shift-snr-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-C Minimum shift time(sec)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-minshift-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atuc-minshift-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "ATU-R Minimum shift time(sec)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-minshift-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "atur-minshift-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Fast Min Rate (kbps)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "fast-min-rate-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "fast-min-rate-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Fast Max Rate (kbps)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "fast-max-rate-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "fast-max-rate-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Interleaved Min Rate (kbps)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-min-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-min-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Interleaved Max Rate (kbps)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-max-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-max-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Interleaved Delay (ms)",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-delay-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-delay-down", buffer2);

            dslam_siemens_hix5300_parse_with_spaces(
                currentSection->start, "Interleaved correction time",
                buffer, sizeof(buffer)
            );
            dslam_siemens_hix5300_parse_speed_up(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-correction-time-up", buffer2);
            dslam_siemens_hix5300_parse_speed_down(buffer, buffer2, sizeof(buffer2));
            dslam_siemens_hix5300_xml_add(node, "interleave-correction-time-down", buffer2);

            xmlAddChild(root_node, node);
            currentSection = currentSection->next;
        } while(currentSection != NULL);
    }
    dslam_siemens_hix5300_section_free(sections);

    xmlNodeDump(psBuf, doc, root_node, 99, 1);
    snprintf(
        d->exec_buffer_post, MDM_DEVICE_EXEC_BUFFER_POST_MAX_LEN,
        "%s", xmlBufferContent(psBuf)
    );
    d->exec_buffer_post_len = xmlBufferLength(psBuf);

    /* Done. */
dslam_siemens_hix5300_get_service_profile_done:
    dslam_siemens_hix5300_xml_free(&doc, &psBuf);
    return;
}

