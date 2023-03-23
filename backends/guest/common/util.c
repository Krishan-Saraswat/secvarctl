/*
 * spdx-license-identifier: apache-2.0
 * copyright 2022-2023 ibm corp.
 */
#include <stdlib.h>
#include <string.h>
#include "endian.h"
#include "err.h"
#include "prlog.h"
#include "util.h"
#include "pseries.h"
#include "common/util.h"

#define X509_TYPE (uint8_t *) "X509"
#define RSA2048_TYPE (uint8_t *) "RSA2048"
#define PKCS7_TYPE (uint8_t *) "PKCS7"
#define SBAT_TYPE (uint8_t *) "SBAT"
#define DELETE_TYPE (uint8_t *) "DELETE-ALL"
#define UNKNOWN_TYPE (uint8_t *) "UNKNOWN"

static uint8_t append[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
static uint8_t replace[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * creates the append header using append flag
 */
uint8_t *
get_append_header (size_t append_flag)
{
  if (append_flag == 1)
    return append;

  return replace;
}

/*
 * extracts the append flag from auth data
 */
size_t
extract_append_header (const uint8_t *auth_info, const size_t auth_len)
{
  if (memcmp (append, auth_info, APPEND_HEADER_LEN) == 0)
    return 1;

  return 0;
}

/*
 * check it whether given signature type is SBAT or not
 */
bool
is_sbat (const uint8_t *signature_type)
{
  size_t len = strlen ((char *) signature_type);

  if (memcmp (signature_type, SBAT_TYPE, len) == 0)
    return true;

  return false;
}

/*
 * validate the SBAT data format
 */
bool
validate_sbat (const uint8_t *sbat_data, size_t sbat_len)
{
  int i = 0, number_of_commas = 0;

  for (i = 0; i < sbat_len; i++)
    {
      if (sbat_data[i] == ',')
        number_of_commas++;
      if (sbat_data[i] == '\n')
        {
          if (number_of_commas != 1)
            return false;
          number_of_commas = 0;
        }
    }

  return true;
}

/*
 * given a string, it will return the corresponding hash_funct info array
 *
 * @param name, the name of the hash function {"sha1", "sha246"...}
 * @param returnfunct, the corresponding hash_funct info array
 * @return success or err number if not a valid hash function name
 */
int
get_hash_function (const char *name, hash_func_t **returnfunct)
{
  int i = 0;

  for (i = 0; i < sizeof (hash_functions) / sizeof (hash_func_t); i++)
    {
      if (memcmp (hash_functions[i].name, name, strlen (name)) == 0)
        {
          *returnfunct = (hash_func_t *) &hash_functions[i];
          return SUCCESS;
        }
    }

  prlog (PR_ERR, "error: invalid hash algorithm %s , hint: use -h { ", name);

  for (i = 0; i < sizeof (hash_functions) / sizeof (hash_func_t); i++)
    {
      if (i == sizeof (hash_functions) / sizeof (hash_func_t) - 1)
        prlog (PR_ERR, "%s }\n", hash_functions[i].name);
      else
        prlog (PR_ERR, "%s, ", hash_functions[i].name);
    }

  return ARG_PARSE_FAIL;
}

/*
 * given a string, it will return the corresponding x509 hash_funct info array
 *
 * @param name, the name of the hash function {"sha1", "sha246"...}
 * @param returnfunct, the corresponding hash_funct info array
 * @return success or err number if not a valid hash function name
 */
int
get_x509_hash_function (const char *name, hash_func_t **returnfunct)
{
  int i = 0;

  for (i = 0; i < sizeof (x509_hash_functions) / sizeof (hash_func_t); i++)
    {
      if (memcmp (x509_hash_functions[i].name, name, strlen (name)) == 0)
        {
          *returnfunct = (hash_func_t *) &x509_hash_functions[i];
          return SUCCESS;
        }
    }

  prlog (PR_ERR, "error: invalid hash algorithm %s , hint: use -h { ", name);

  for (i = 0; i < sizeof (x509_hash_functions) / sizeof (hash_func_t); i++)
    {
      if (i == sizeof (x509_hash_functions) / sizeof (hash_func_t) - 1)
        prlog (PR_ERR, "%s }\n", x509_hash_functions[i].name);
      else
        prlog (PR_ERR, "%s, ", x509_hash_functions[i].name);
    }

  return ARG_PARSE_FAIL;
}

/*
 * finds format type given by guid
 *
 * @param type uuid_t of guid of file
 * @return string of format type, "UNKNOWN" if type doesnt match any known formats
 */
uint8_t *
get_signature_type (const uuid_t type)
{
  for (int i = 0; i < sizeof (x509_hash_functions) / sizeof (hash_func_t); i++)
    {
      if (uuid_equals (&type, x509_hash_functions[i].guid))
        return (uint8_t *) x509_hash_functions[i].name;
    }

  for (int i = 0; i < sizeof (hash_functions) / sizeof (hash_func_t); i++)
    {
      if (uuid_equals (&type, hash_functions[i].guid))
        return (uint8_t *) hash_functions[i].name;
    }

  if (uuid_equals (&type, &PKS_CERT_X509_GUID))
    return X509_TYPE;
  else if (uuid_equals (&type, &PKS_CERT_RSA2048_GUID))
    return RSA2048_TYPE;
  else if (uuid_equals (&type, &AUTH_CERT_TYPE_PKCS7_GUID))
    return PKCS7_TYPE;
  else if (uuid_equals (&type, &PKS_CERT_SBAT_GUID))
    return SBAT_TYPE;
  else if (uuid_equals (&type, &PKS_CERT_DELETE_GUID))
    return DELETE_TYPE;

  return UNKNOWN_TYPE;
}

/*
 * check it whether given signature type is hash or not
 */
bool
is_hash (const uint8_t *signature_type)
{
  int i = 0;
  size_t len = strlen ((char *) signature_type);

  for (i = 0; i < sizeof (hash_functions) / sizeof (hash_func_t); i++)
    {
      if (memcmp (signature_type, hash_functions[i].name, len) == 0)
        return true;
    }

  for (i = 0; i < sizeof (x509_hash_functions) / sizeof (hash_func_t); i++)
    {
      if (memcmp (signature_type, x509_hash_functions[i].name, len) == 0)
        return true;
    }

  return false;
}

/*
 * check it whether given signature type is x509 or not
 */
bool
is_cert (const uint8_t *signature_type)
{
  size_t len = strlen ((char *) signature_type);

  if (memcmp (signature_type, X509_TYPE, len) == 0)
    return true;

  return false;
}

/*
 * check it whether given signature type is PKCS7 or not
 */
bool
is_pkcs7 (const uint8_t *signature_type)
{
  size_t len = strlen ((char *) signature_type);

  if (memcmp (signature_type, PKCS7_TYPE, len) == 0)
    return true;

  return false;
}

/*
 * check it whether given signature type is DELETE-ALL or not
 */
bool
is_delete (const uint8_t *signature_type)
{
  size_t len = strlen ((char *) signature_type);

  if (memcmp (signature_type, DELETE_TYPE, len) == 0)
    return true;

  return false;
}

/*
 * validates the signature type
 */
bool
validate_signature_type (const uint8_t *signature_type)
{
  if (is_hash (signature_type))
    return true;

  if (is_cert (signature_type) || is_sbat (signature_type) ||
      is_delete (signature_type))
    return true;

  return false;
}

/*
 * checks to see if string is a valid variable name
 * @param var variable name
 * @return SUCCESS or error code
 */
bool
is_secure_boot_variable (const char *var)
{
  int i = 0;

  for (i = 0; i < defined_sb_variable_len; i++)
    {
      if (memcmp ((char *) defined_sb_variables[i], var, strlen (var)) == 0)
        return true;
    }

  return false;
}

/*
 * expand char to wide character size , for edk2 since esl's use double wides
 *
 * @param key ,key name
 * @param keylen, length of key
 * @return the new keylen with double length, remember to unalloc
 */
uint8_t *
get_wide_character (const uint8_t *key, const size_t keylen)
{
  int i;
  uint8_t *str;

  str = zalloc (keylen * 3);
  if (!str)
    return NULL;

  memset (str, 0x00, keylen * 3);
  for (i = 0; i < keylen * 2; key++)
    {
      str[i++] = *key;
      str[i++] = '\0';
    }

  return str;
}

/*
 * Extracts size of the PKCS7 signed data embedded in the
 * auth Header.
 */
size_t
extract_pkcs7_len (const auth_info_t *auth)
{
  uint32_t da_length;
  size_t size;

  da_length = le32_to_cpu (auth->auth_cert.hdr.da_length);
  size = da_length - (sizeof (auth->auth_cert.hdr.da_length) +
                      sizeof (auth->auth_cert.hdr.a_revision) +
                      sizeof (auth->auth_cert.hdr.a_certificate_type) +
                      sizeof (auth->auth_cert.cert_type));

  return size;
}