/*
 * tests/create/kdb5_mkdums.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Edit a KDC database.
 */

#include "k5-int.h"
#include "com_err.h"
#include <ss/ss.h>
#include <stdio.h>


#define REALM_SEP	'@'
#define REALM_SEP_STR	"@"

struct mblock {
    krb5_deltat max_life;
    krb5_deltat max_rlife;
    krb5_timestamp expiration;
    krb5_flags flags;
    krb5_kvno mkvno;
} mblock = {				/* XXX */
    KRB5_KDB_MAX_LIFE,
    KRB5_KDB_MAX_RLIFE,
    KRB5_KDB_EXPIRATION,
    KRB5_KDB_DEF_FLAGS,
    1
};

int set_dbname_help PROTOTYPE((char *, char *));

static void
usage(who, status)
char *who;
int status;
{
    fprintf(stderr,
	    "usage: %s -p prefix -n num_to_create [-d dbpathname] [-r realmname]\n",
	    who);
    fprintf(stderr, "\t [-D depth] [-k enctype] [-M mkeyname]\n");

    exit(status);
}

krb5_keyblock master_keyblock;
krb5_principal master_princ;
krb5_db_entry master_entry;
krb5_encrypt_block master_encblock;
krb5_pointer master_random;
krb5_context test_context;

static char *progname;
static char *cur_realm = 0;
static char *mkey_name = 0;
static char *mkey_password = 0;
static krb5_boolean manual_mkey = FALSE;
static krb5_boolean dbactive = FALSE;

void
quit()
{
    krb5_error_code retval = krb5_db_fini(test_context);
    memset((char *)master_keyblock.contents, 0, master_keyblock.length);
    if (retval) {
	com_err(progname, retval, "while closing database");
	exit(1);
    }
    exit(0);
}

void add_princ PROTOTYPE((krb5_context, char *));

void
main(argc, argv)
int argc;
char *argv[];
{
    extern char *optarg;	
    int optchar, i, n;
    char tmp[4096], tmp2[BUFSIZ], *str_newprinc;

    krb5_error_code retval;
    char *dbname = 0;
    int enctypedone = 0;
    register krb5_cryptosystem_entry *csentry;
    extern krb5_kt_ops krb5_ktf_writable_ops;
    int num_to_create;
    char principal_string[BUFSIZ];
    char *suffix = 0;
    int depth;

    krb5_init_context(&test_context);
    krb5_init_ets(test_context);

    if (strrchr(argv[0], '/'))
	argv[0] = strrchr(argv[0], '/')+1;

    progname = argv[0];

    memset(principal_string, 0, sizeof(principal_string));
    num_to_create = 0;
    depth = 1;

    while ((optchar = getopt(argc, argv, "D:P:p:n:d:r:k:M:e:m")) != EOF) {
	switch(optchar) {
	case 'D':
	    depth = atoi(optarg);       /* how deep to go */
	    break;
        case 'P':		/* Only used for testing!!! */
	    mkey_password = optarg;
	    break;
	case 'p':                       /* prefix name to create */
	    strcpy(principal_string, optarg);
	    suffix = principal_string + strlen(principal_string);
	    break;
	case 'n':                        /* how many to create */
	    num_to_create = atoi(optarg);
	    break;
	case 'd':			/* set db name */
	    dbname = optarg;
	    break;
	case 'r':
	    cur_realm = optarg;
	    break;
	case 'k':
	    master_keyblock.enctype = atoi(optarg);
	    enctypedone++;
	    break;
	case 'M':			/* master key name in DB */
	    mkey_name = optarg;
	    break;
	case 'm':
	    manual_mkey = TRUE;
	    break;
	case '?':
	default:
	    usage(progname, 1);
	    /*NOTREACHED*/
	}
    }

    if (!(num_to_create && suffix)) usage(progname, 1);


    if (retval = krb5_kt_register(test_context, &krb5_ktf_writable_ops)) {
	com_err(progname, retval,
		"while registering writable key table functions");
	exit(1);
    }

    if (!enctypedone)
	master_keyblock.enctype = DEFAULT_KDC_ENCTYPE;

    if (!valid_enctype(master_keyblock.enctype)) {
	com_err(progname, KRB5_PROG_ETYPE_NOSUPP,
		"while setting up enctype %d", master_keyblock.enctype);
	exit(1);
    }

    krb5_use_enctype(test_context, &master_encblock, master_keyblock.enctype);
    csentry = master_encblock.crypto_entry;

    if (!dbname)
	dbname = DEFAULT_KDB_FILE;	/* XXX? */

    if (!cur_realm) {
	if (retval = krb5_get_default_realm(test_context, &cur_realm)) {
	    com_err(progname, retval, "while retrieving default realm name");
	    exit(1);
	}	    
    }
    if (retval = set_dbname_help(progname, dbname))
	exit(retval);

    for (n = 1; n <= num_to_create; n++) {
      /* build the new principal name */
      /* we can't pick random names because we need to generate all the names 
	 again given a prefix and count to test the db lib and kdb */
      (void) sprintf(suffix, "%d", n);
      (void) sprintf(tmp, "%s-DEPTH-1", principal_string);
      str_newprinc = tmp;
      add_princ(test_context, str_newprinc);

      for (i = 2; i <= depth; i++) {
	tmp2[0] = '\0';
	(void) sprintf(tmp2, "/%s-DEPTH-%d", principal_string, i);
	strcat(tmp, tmp2);
	str_newprinc = tmp;
	add_princ(test_context, str_newprinc);
      }
    }

    (void) (*csentry->finish_key)(&master_encblock);
    (void) (*csentry->finish_random_key)(&master_random);
    retval = krb5_db_fini(test_context);
    memset((char *)master_keyblock.contents, 0, master_keyblock.length);
    if (retval && retval != KRB5_KDB_DBNOTINITED) {
	com_err(progname, retval, "while closing database");
	exit(1);
    }
    exit(0);
}

void
add_princ(context, str_newprinc)
    krb5_context 	  context;
    char 		* str_newprinc;
{
    krb5_error_code 	  retval;
    krb5_principal 	  newprinc;
    krb5_db_entry 	  newentry;
    char 		  princ_name[4096];

    memset((char *)&newentry, 0, sizeof(newentry));
    sprintf(princ_name, "%s@%s", str_newprinc, cur_realm);
    if (retval = krb5_parse_name(context, princ_name, &newprinc)) {
      com_err(progname, retval, "while parsing '%s'", princ_name);
      return;
    }

    /* Add basic data */
    newentry.len = KRB5_KDB_V1_BASE_LENGTH;
    newentry.attributes = mblock.flags;
    newentry.max_life = mblock.max_life;
    newentry.max_renewable_life = mblock.max_rlife;
    newentry.expiration = mblock.expiration;
    newentry.pw_expiration = mblock.expiration;
    
    /* Add princ to db entry */
    if (retval = krb5_copy_principal(context, newprinc, &newentry.princ)) {
      	com_err(progname, retval, "while encoding princ to db entry for '%s'", 
	        princ_name);
	goto error;
    }

    {	/* Add mod princ to db entry */
	krb5_tl_mod_princ mod_princ;

    	mod_princ.mod_princ = master_princ;
    	if (retval = krb5_timeofday(context, &mod_princ.mod_date)) {
	    com_err(progname, retval, "while fetching date");
	    goto error;
        }
	if(retval=krb5_dbe_encode_mod_princ_data(context,&mod_princ,&newentry)){
	    com_err(progname, retval, "while encoding mod_princ data");
	    goto error;
	}
    }

    {   /* Add key and salt data to db entry */
        krb5_data pwd, salt;
        krb5_keyblock key;

        if (retval = krb5_principal2salt(context, newprinc, &salt)) {
	    com_err(progname, retval, "while converting princ to salt for '%s'",
		    princ_name);
	    goto error;
        }

    	pwd.length = strlen(princ_name);
    	pwd.data = princ_name;  /* must be able to regenerate */
    	if (retval = krb5_string_to_key(context, &master_encblock, 
				        &key, &pwd, &salt)) {
	    com_err(progname,retval,"while converting password to key for '%s'",
		    princ_name);
	    krb5_xfree(salt.data);
	    goto error;
	}
	krb5_xfree(salt.data);

	if (retval = krb5_dbe_create_key_data(context, &newentry)) {
	    com_err(progname, retval, "while creating key_data for '%s'",
		    princ_name);
            free(key.contents);
	    goto error;
        }

        if (retval = krb5_dbekd_encrypt_key_data(context,&master_encblock, &key,
				               NULL, 1, newentry.key_data)) {
    	    com_err(progname, retval, "while encrypting key for '%s'", 
		    princ_name);
            free(key.contents);
	    goto error;
        }
        free(key.contents);
    }

    {
	int one = 1;

    	if (retval = krb5_db_put_principal(context, &newentry, &one)) {
	    com_err(progname, retval, "while storing principal date");
	    goto error;
    	}
    	if (one != 1) {
	   com_err(progname,0,"entry not stored in database (unknown failure)");
	    goto error;
    	}
    }

    fprintf(stdout, "Added %s to database\n", princ_name);

error: /* Do cleanup of newentry regardless of error */
    krb5_dbe_free_contents(context, &newentry);
    return;
}

int
set_dbname_help(pname, dbname)
char *pname;
char *dbname;
{
    krb5_error_code retval;
    int nentries;
    krb5_boolean more;
    register krb5_cryptosystem_entry *csentry;
    krb5_data pwd, scratch;

    csentry = master_encblock.crypto_entry;

    if (retval = krb5_db_set_name(test_context, dbname)) {
	com_err(pname, retval, "while setting active database to '%s'",
		dbname);
	return(1);
    }
    /* assemble & parse the master key name */

    if (retval = krb5_db_setup_mkey_name(test_context, mkey_name, cur_realm, 0,
					 &master_princ)) {
	com_err(pname, retval, "while setting up master key name");
	return(1);
    }
    if (mkey_password) {
	pwd.data = mkey_password;
	pwd.length = strlen(mkey_password);
	retval = krb5_principal2salt(test_context, master_princ, &scratch);
	if (retval) {
	    com_err(pname, retval, "while calculated master key salt");
	    return(1);
	}
	if (retval = krb5_string_to_key(test_context, &master_encblock, 
				    &master_keyblock, &pwd, &scratch)) {
	    com_err(pname, retval,
		    "while transforming master key from password");
	    return(1);
	}
	free(scratch.data);
    } else {
	if (retval = krb5_db_fetch_mkey(test_context, master_princ, 
					&master_encblock, manual_mkey, 
					FALSE, 0, NULL, &master_keyblock)) {
	    com_err(pname, retval, "while reading master key");
	    return(1);
	}
    }
    if (retval = krb5_db_init(test_context)) {
	com_err(pname, retval, "while initializing database");
	return(1);
    }
    if (retval = krb5_db_verify_master_key(test_context, master_princ, 
					   &master_keyblock, &master_encblock)){
	com_err(pname, retval, "while verifying master key");
	(void) krb5_db_fini(test_context);
	return(1);
    }
    nentries = 1;
    if (retval = krb5_db_get_principal(test_context, master_princ, 
				       &master_entry, &nentries, &more)) {
	com_err(pname, retval, "while retrieving master entry");
	(void) krb5_db_fini(test_context);
	return(1);
    } else if (more) {
	com_err(pname, KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE,
		"while retrieving master entry");
	(void) krb5_db_fini(test_context);
	return(1);
    } else if (!nentries) {
	com_err(pname, KRB5_KDB_NOENTRY, "while retrieving master entry");
	(void) krb5_db_fini(test_context);
	return(1);
    }

    if (retval = (*csentry->process_key)(&master_encblock,
					 &master_keyblock)) {
	com_err(pname, retval, "while processing master key");
	(void) krb5_db_fini(test_context);
	return(1);
    }
    if (retval = (*csentry->init_random_key)(&master_keyblock,
					     &master_random)) {
	com_err(pname, retval, "while initializing random key generator");
	(void) (*csentry->finish_key)(&master_encblock);
	(void) krb5_db_fini(test_context);
	return(1);
    }
    mblock.max_life = master_entry.max_life;
    mblock.max_rlife = master_entry.max_renewable_life;
    mblock.expiration = master_entry.expiration;

    /* don't set flags, master has some extra restrictions */
    mblock.mkvno = master_entry.key_data[0].key_data_kvno;

    krb5_db_free_principal(test_context, &master_entry, nentries);
    dbactive = TRUE;
    return 0;
}

