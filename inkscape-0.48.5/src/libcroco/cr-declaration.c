/* -*- Mode: C; indent-tabs-mode: ni; c-basic-offset: 8 -*- */

/*
 * This file is part of The Croco Library
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * Author: Dodji Seketeli.
 * See COPYRIGHTS file for copyright information.
 */


#include <string.h>
#include "cr-declaration.h"
#include "cr-statement.h"
#include "cr-parser.h"

/**
 *@file
 *The definition of the #CRDeclaration class.
 */

/**
 *Dumps (serializes) one css declaration to a file.
 *@param a_this the current instance of #CRDeclaration.
 *@param a_fp the destination file pointer.
 *@param a_indent the number of indentation white char. 
 */
static void
dump (CRDeclaration * a_this, FILE * a_fp, glong a_indent)
{
        g_return_if_fail (a_this);

        gchar *str = cr_declaration_to_string (a_this, a_indent);
        if (str) {
                fprintf (a_fp, "%s", str);
                g_free (str);
                str = NULL;
        }
}

/**
 *Constructor of #CRDeclaration.
 *@param a_property the property string of the declaration
 *@param a_value the value expression of the declaration.
 *@return the newly built instance of #CRDeclaration, or NULL in
 *case of error.
 */
CRDeclaration *
cr_declaration_new (CRStatement * a_statement,
                    CRString * a_property, CRTerm * a_value)
{
        g_return_val_if_fail (a_property, NULL);

        if (a_statement)
                g_return_val_if_fail (a_statement
                                      && ((a_statement->type == RULESET_STMT)
                                          || (a_statement->type
                                              == AT_FONT_FACE_RULE_STMT)
                                          || (a_statement->type
                                              == AT_PAGE_RULE_STMT)), NULL);

        CRDeclaration * result = (CRDeclaration *)g_try_malloc (sizeof (CRDeclaration));
        if (!result) {
                cr_utils_trace_info ("Out of memory");
                return NULL;
        }
        memset (result, 0, sizeof (CRDeclaration));
        result->property = a_property;
        result->value = a_value;

        if (a_value) {
                cr_term_ref (a_value);
        }
        result->parent_statement = a_statement;
        return result;
}

/**
 *Parses a text buffer that contains
 *a css declaration.
 *
 *@param a_statement the parent css2 statement of this
 *this declaration. Must be non NULL and of type
 *RULESET_STMT (must be a ruleset).
 *@param a_str the string that contains the statement.
 *@param a_enc the encoding of a_str.
 *@return the parsed declaration, or NULL in case of error.
 */
CRDeclaration *
cr_declaration_parse_from_buf (CRStatement * a_statement,
                               const guchar * a_str, enum CREncoding a_enc)
{
        enum CRStatus status = CR_OK;
        CRTerm *value = NULL;
        CRString *property = NULL;
        CRDeclaration *result = NULL;
        gboolean important = FALSE;

        g_return_val_if_fail (a_str, NULL);
        if (a_statement)
                g_return_val_if_fail (a_statement->type == RULESET_STMT,
                                      NULL);

        CRParser *parser = (CRParser *)
		        cr_parser_new_from_buf ((guchar*)a_str,
				  strlen ((char *)a_str), a_enc, FALSE);
        g_return_val_if_fail (parser, NULL);

        status = cr_parser_try_to_skip_spaces_and_comments (parser);
        if (status != CR_OK)
                goto cleanup;

        status = cr_parser_parse_declaration (parser, &property,
                                              &value, &important);
        if (status != CR_OK || !property)
                goto cleanup;

        result = cr_declaration_new (a_statement, property, value);
        if (result) {
                property = NULL;
                value = NULL;
                result->important = important;
        }

      cleanup:

        if (parser) {
                cr_parser_destroy (parser);
                parser = NULL;
        }

        if (property) {
                cr_string_destroy (property);
                property = NULL;
        }

        if (value) {
                cr_term_destroy (value);
                value = NULL;
        }

        return result;
}

/**
 *Parses a ';' separated list of properties declaration.
 *@param a_str the input buffer that contains the list of declaration to
 *parse.
 *@param a_enc the encoding of a_str
 *@return the parsed list of declaration, NULL if parsing failed.
 */
CRDeclaration *
cr_declaration_parse_list_from_buf (const guchar * a_str,
                                    enum CREncoding a_enc)
{

        enum CRStatus status = CR_OK;
        CRTerm *value = NULL;
        CRString *property = NULL;
        CRDeclaration *result = NULL,
                *cur_decl = NULL;
        CRTknzr *tokenizer = NULL;
        gboolean important = FALSE;

        g_return_val_if_fail (a_str, NULL);

        CRParser *parser = (CRParser *)cr_parser_new_from_buf
		      ((guchar*)a_str, strlen ((char *)a_str), a_enc, FALSE);
        g_return_val_if_fail (parser, NULL);
        status = cr_parser_get_tknzr (parser, &tokenizer);
        if (status != CR_OK || !tokenizer) {
                if (status == CR_OK)
                        status = CR_ERROR;
                goto cleanup;
        }
        status = cr_parser_try_to_skip_spaces_and_comments (parser);
        if (status != CR_OK)
                goto cleanup;

        status = cr_parser_parse_declaration (parser, &property,
                                              &value, &important);
        if (status != CR_OK || !property) {
                if (status != CR_OK)
                        status = CR_ERROR;
                goto cleanup;
        }
        result = cr_declaration_new (NULL, property, value);
        if (result) {
                property = NULL;
                value = NULL;
                result->important = important;
        }
        /*now, go parse the other declarations */
        for (;;) {
                guint32 c = 0;

                cr_parser_try_to_skip_spaces_and_comments (parser);
                status = cr_tknzr_peek_char (tokenizer, &c);
                if (status != CR_OK) {
                        if (status == CR_END_OF_INPUT_ERROR) 
                                status = CR_OK;
                        goto cleanup;
                }
                if (c == ';') {
                        status = cr_tknzr_read_char (tokenizer, &c);
                } else {
                        cr_tknzr_read_char (tokenizer, &c);
                        continue; // try to keep reading until we reach the end or a ;
                }
                important = FALSE;
                cr_parser_try_to_skip_spaces_and_comments (parser);
                status = cr_parser_parse_declaration (parser, &property,
                                                      &value, &important);
                if (status != CR_OK || !property) {
                        if (status == CR_END_OF_INPUT_ERROR) {
                                status = CR_OK; // simply the end of input, do not delete what we got so far, just finish
                                break; 
                        } else {
                                continue; // even if one declaration is broken, it's no reason to discard others (see http://www.w3.org/TR/CSS21/syndata.html#declaration)
								}
                }
                cur_decl = cr_declaration_new (NULL, property, value);
                if (cur_decl) {
                        cur_decl->important = important;
                        result = cr_declaration_append (result, cur_decl);
                        property = NULL;
                        value = NULL;
                        cur_decl = NULL;
                } else {
                        break;
                }
        }

      cleanup:

        if (parser) {
                cr_parser_destroy (parser);
                parser = NULL;
        }

        if (property) {
                cr_string_destroy (property);
                property = NULL;
        }

        if (value) {
                cr_term_destroy (value);
                value = NULL;
        }

        if (status != CR_OK && result) {
                cr_declaration_destroy (result);
                result = NULL;
        }
        return result;
}

/**
 *Appends a new declaration to the current declarations list.
 *@param a_this the current declaration list.
 *@param a_new the declaration to append.
 *@return the declaration list with a_new appended to it, or NULL
 *in case of error.
 */
CRDeclaration *
cr_declaration_append (CRDeclaration * a_this, CRDeclaration * a_new)
{
        CRDeclaration *cur = NULL;

        g_return_val_if_fail (a_new, NULL);

        if (!a_this)
                return a_new;

        for (cur = a_this; cur && cur->next; cur = cur->next) ;

        cur->next = a_new;
        a_new->prev = cur;

        return a_this;
}

/**
 *Unlinks the declaration from the declaration list.
 *@param a_decl the declaration to unlink.
 *@return a pointer to the unlinked declaration in
 *case of a successfull completion, NULL otherwise.
 */
CRDeclaration *
cr_declaration_unlink (CRDeclaration * a_decl)
{
        CRDeclaration *result = a_decl;

        g_return_val_if_fail (result, NULL);

        /*
         *some sanity checks first
         */
        if (a_decl->prev) {
                g_return_val_if_fail (a_decl->prev->next == a_decl, NULL);

        }
        if (a_decl->next) {
                g_return_val_if_fail (a_decl->next->prev == a_decl, NULL);
        }

        /*
         *now, the real unlinking job.
         */
        if (a_decl->prev) {
                a_decl->prev->next = a_decl->next;
        }
        if (a_decl->next) {
                a_decl->next->prev = a_decl->prev;
        }
        if (a_decl->parent_statement) {
                CRDeclaration **children_decl_ptr = NULL;

                switch (a_decl->parent_statement->type) {
                case RULESET_STMT:
                        if (a_decl->parent_statement->kind.ruleset) {
                                children_decl_ptr =
                                        &a_decl->parent_statement->
                                        kind.ruleset->decl_list;
                        }

                        break;

                case AT_FONT_FACE_RULE_STMT:
                        if (a_decl->parent_statement->kind.font_face_rule) {
                                children_decl_ptr =
                                        &a_decl->parent_statement->
                                        kind.font_face_rule->decl_list;
                        }
                        break;
                case AT_PAGE_RULE_STMT:
                        if (a_decl->parent_statement->kind.page_rule) {
                                children_decl_ptr =
                                        &a_decl->parent_statement->
                                        kind.page_rule->decl_list;
                        }

                default:
                        break;
                }
                if (children_decl_ptr
                    && *children_decl_ptr && *children_decl_ptr == a_decl)
                        *children_decl_ptr = (*children_decl_ptr)->next;
        }

        a_decl->next = NULL;
        a_decl->prev = NULL;
        a_decl->parent_statement = NULL;

        return result;
}

/**
 *prepends a declaration to the current declaration list.
 *@param a_this the current declaration list.
 *@param a_new the declaration to prepend.
 *@return the list with a_new prepended or NULL in case of error.
 */
CRDeclaration *
cr_declaration_prepend (CRDeclaration * a_this, CRDeclaration * a_new)
{
        CRDeclaration *cur = NULL;

        g_return_val_if_fail (a_new, NULL);

        if (!a_this)
                return a_new;

        a_this->prev = a_new;
        a_new->next = a_this;

        for (cur = a_new; cur && cur->prev; cur = cur->prev) ;

        return cur;
}

/**
 *Appends a declaration to the current declaration list.
 *@param a_this the current declaration list.
 *@param a_prop the property string of the declaration to append.
 *@param a_value the value of the declaration to append.
 *@return the list with the new property appended to it, or NULL in
 *case of an error.
 */
CRDeclaration *
cr_declaration_append2 (CRDeclaration * a_this,
                        CRString * a_prop, CRTerm * a_value)
{
        CRDeclaration *new_elem = NULL;

        if (a_this) {
                new_elem = cr_declaration_new (a_this->parent_statement,
                                               a_prop, a_value);
        } else {
                new_elem = cr_declaration_new (NULL, a_prop, a_value);
        }

        g_return_val_if_fail (new_elem, NULL);

        return cr_declaration_append (a_this, new_elem);
}

/**
 *Dumps a declaration list to a file.
 *@param a_this the current instance of #CRDeclaration.
 *@param a_fp the destination file.
 *@param a_indent the number of indentation white char.
 */
void
cr_declaration_dump (CRDeclaration * a_this, FILE * a_fp, glong a_indent,
                     gboolean a_one_per_line)
{
        CRDeclaration *cur = NULL;

        g_return_if_fail (a_this);

        for (cur = a_this; cur; cur = cur->next) {
                if (cur->prev) {
                        if (a_one_per_line == TRUE)
                                fprintf (a_fp, ";\n");
                        else
                                fprintf (a_fp, "; ");
                }
                dump (cur, a_fp, a_indent);
        }
}

/**
 *Dumps the first declaration of the declaration list to a file.
 *@param a_this the current instance of #CRDeclaration.
 *@param a_fp the destination file.
 *@param a_indent the number of indentation white char.
 */
void
cr_declaration_dump_one (CRDeclaration * a_this, FILE * a_fp, glong a_indent)
{
        g_return_if_fail (a_this);

        dump (a_this, a_fp, a_indent);
}

/**
 *Serializes the declaration into a string
 *@param a_this the current instance of #CRDeclaration.
 *@param a_indent the number of indentation white char
 *to put before the actual serialisation.
 */
gchar *
cr_declaration_to_string (CRDeclaration * a_this, gulong a_indent)
{
        GString *stringue = NULL;

        gchar *str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this, NULL);

	stringue = g_string_new (NULL);

	if (a_this->property 
	    && a_this->property->stryng
	    && a_this->property->stryng->str) {
		str = g_strndup (a_this->property->stryng->str,
				 a_this->property->stryng->len);
		if (str) {
			cr_utils_dump_n_chars2 (' ', stringue, 
						a_indent);
			g_string_append (stringue, str);
			g_free (str);
			str = NULL;
		} else
                        goto error;

                if (a_this->value) {
                        guchar *value_str = NULL;

                        value_str = cr_term_to_string (a_this->value);
                        if (value_str) {
                                g_string_append_printf (stringue, " : %s",
                                                        value_str);
                                g_free (value_str);
                        } else
                                goto error;
                }
                if (a_this->important == TRUE) {
                        g_string_append_printf (stringue, " %s",
                                                "!important");
                }
        }
        if (stringue && stringue->str) {
                result = stringue->str;
                g_string_free (stringue, FALSE);
        }
        return result;

      error:
        if (stringue) {
                g_string_free (stringue, TRUE);
                stringue = NULL;
        }
        if (str) {
                g_free (str);
                str = NULL;
        }

        return result;
}

/**
 *Serializes the declaration list into a string
 *@param a_this the current instance of #CRDeclaration.
 *@param a_indent the number of indentation white char
 *to put before the actual serialisation.
 */
guchar *
cr_declaration_list_to_string (CRDeclaration * a_this, gulong a_indent)
{
        CRDeclaration *cur = NULL;
        GString *stringue = NULL;
        gchar *str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this, NULL);

        stringue = g_string_new (NULL);

        for (cur = a_this; cur; cur = cur->next) {
                str = cr_declaration_to_string (cur, a_indent);
                if (str) {
                        g_string_append_printf (stringue, "%s;", str);
                        g_free (str);
                } else
                        break;
        }
        if (stringue && stringue->str) {
                result = stringue->str;
                g_string_free (stringue, FALSE);
        }

        return (guchar *)result;
}

/**
 *Serializes the declaration list into a string
 *@param a_this the current instance of #CRDeclaration.
 *@param a_indent the number of indentation white char
 *to put before the actual serialisation.
 */
guchar *
cr_declaration_list_to_string2 (CRDeclaration * a_this,
                                gulong a_indent, gboolean a_one_decl_per_line)
{
        CRDeclaration *cur = NULL;
        GString *stringue = NULL;
        gchar *str = NULL,
                *result = NULL;

        g_return_val_if_fail (a_this, NULL);

        stringue = g_string_new (NULL);

        for (cur = a_this; cur; cur = cur->next) {
                str = cr_declaration_to_string (cur, a_indent);
                if (str) {
                        if (a_one_decl_per_line == TRUE) {
                                if (cur->next)
                                        g_string_append_printf (stringue,
                                                                "%s;\n", str);
                                else
                                        g_string_append (stringue,
                                                         str);
                        } else {
                                if (cur->next)
                                        g_string_append_printf (stringue,
                                                                "%s;", str);
                                else
                                        g_string_append (stringue,
                                                         str);
                        }
                        g_free (str);
                } else
                        break;
        }
        if (stringue && stringue->str) {
                result = stringue->str;
                g_string_free (stringue, FALSE);
        }

        return (guchar *)result;
}

/**
 *Return the number of properties in the declaration;
 *@param a_this the current instance of #CRDeclaration.
 *@return number of properties in the declaration list.
 */
gint
cr_declaration_nr_props (CRDeclaration * a_this)
{
        CRDeclaration *cur = NULL;
        int nr = 0;

        g_return_val_if_fail (a_this, -1);

        for (cur = a_this; cur; cur = cur->next)
                nr++;
        return nr;
}

/**
 *Use an index to get a CRDeclaration from the declaration list.
 *@param a_this the current instance of #CRDeclaration.
 *@param itemnr the index into the declaration list.
 *@return CRDeclaration at position itemnr, if itemnr > number of declarations - 1,
 *it will return NULL.
 */
CRDeclaration *
cr_declaration_get_from_list (CRDeclaration * a_this, int itemnr)
{
        CRDeclaration *cur = NULL;
        int nr = 0;

        g_return_val_if_fail (a_this, NULL);

        for (cur = a_this; cur; cur = cur->next)
                if (nr++ == itemnr)
                        return cur;
        return NULL;
}

/**
 *Use property name to get a CRDeclaration from the declaration list.
 *@param a_this the current instance of #CRDeclaration.
 *@param a_prop the property name to search for.
 *@return CRDeclaration with property name a_prop, or NULL if not found.
 */
CRDeclaration *
cr_declaration_get_by_prop_name (CRDeclaration * a_this,
                                 const guchar * a_prop)
{
        CRDeclaration *cur = NULL;

        g_return_val_if_fail (a_this, NULL);
        g_return_val_if_fail (a_prop, NULL);

        for (cur = a_this; cur; cur = cur->next) {
		if (cur->property 
		    && cur->property->stryng
		    && cur->property->stryng->str) {
			if (!strcmp (cur->property->stryng->str, 
				     (char *)a_prop)) {
				return cur;
			}
		}
	}
        return NULL;
}

/**
 *Increases the ref count of the current instance of #CRDeclaration.
 *@param a_this the current instance of #CRDeclaration.
 */
void
cr_declaration_ref (CRDeclaration * a_this)
{
        g_return_if_fail (a_this);

        a_this->ref_count++;
}

/**
 *Decrements the ref count of the current instance of #CRDeclaration.
 *If the ref count reaches zero, the current instance of #CRDeclaration
 *if destroyed.
 *@param a_this the current instance of #CRDeclaration.
 *@return TRUE if the current instance of #CRDeclaration has been destroyed
 *(ref count reached zero), FALSE otherwise.
 */
gboolean
cr_declaration_unref (CRDeclaration * a_this)
{
        g_return_val_if_fail (a_this, FALSE);

        if (a_this->ref_count) {
                a_this->ref_count--;
        }

        if (a_this->ref_count == 0) {
                cr_declaration_destroy (a_this);
                return TRUE;
        }
        return FALSE;
}

/**
 *Destructor of the declaration list.
 *@param a_this the current instance of #CRDeclaration.
 */
void
cr_declaration_destroy (CRDeclaration * a_this)
{
        CRDeclaration *cur = NULL;

        g_return_if_fail (a_this);

        /*
         *Go get the tail of the list.
         *Meanwhile, free each property/value pair contained in the list.
         */
        for (cur = a_this; cur && cur->next; cur = cur->next) {
                if (cur->property) {
                        cr_string_destroy (cur->property);
                        cur->property = NULL;
                }

                if (cur->value) {
                        cr_term_destroy (cur->value);
                        cur->value = NULL;
                }
        }

        if (cur) {
                if (cur->property) {
                        cr_string_destroy (cur->property);
                        cur->property = NULL;
                }

                if (cur->value) {
                        cr_term_destroy (cur->value);
                        cur->value = NULL;
                }
        }

        /*in case the list contains only one element */
        if (cur && !cur->prev) {
                g_free (cur);
                return;
        }

        /*walk backward the list and free each "next" element */
        for (cur = cur->prev; cur && cur->prev; cur = cur->prev) {
                if (cur->next) {
                        g_free (cur->next);
                        cur->next = NULL;
                }
        }

        if (!cur)
                return;

        if (cur->next) {
                g_free (cur->next);
                cur->next = NULL;
        }

        g_free (cur);
}