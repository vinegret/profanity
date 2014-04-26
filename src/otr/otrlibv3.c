/*
 * otrlibv3.c
 *
 * Copyright (C) 2012 - 2014 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <libotr/proto.h>
#include <libotr/privkey.h>
#include <libotr/message.h>

#include "ui/ui.h"

OtrlPolicy
otrlib_policy(void)
{
    return OTRL_POLICY_ALLOW_V1 | OTRL_POLICY_ALLOW_V2 ;
}

char *
otrlib_start_query(void)
{
    return "?OTR?v2? This user has requested an Off-the-Record private conversation. However, you do not have a plugin to support that. See http://otr.cypherpunks.ca/ for more information.";
}

static int
cb_display_otr_message(void *opdata, const char *accountname,
    const char *protocol, const char *username, const char *msg)
{
    cons_show_error("%s", msg);
    return 0;
}

void
otrlib_init_ops(OtrlMessageAppOps *ops)
{
    ops->display_otr_message = cb_display_otr_message;
}

ConnContext *
otrlib_context_find(OtrlUserState user_state, const char * const recipient, char *jid)
{
    return otrl_context_find(user_state, recipient, jid, "xmpp", 0, NULL, NULL, NULL);
}

void
otrlib_end_session(OtrlUserState user_state, const char * const recipient, char *jid, OtrlMessageAppOps *ops)
{
    ConnContext *context = otrl_context_find(user_state, recipient, jid, "xmpp",
        0, NULL, NULL, NULL);

    if (context != NULL) {
        otrl_message_disconnect(user_state, ops, NULL, jid, "xmpp", recipient);
    }
}

gcry_error_t
otrlib_encrypt_message(OtrlUserState user_state, OtrlMessageAppOps *ops, char *jid, const char * const to,
    const char * const message, char **newmessage)
{
    gcry_error_t err;
    err = otrl_message_sending(
        user_state,
        ops,
        NULL,
        jid,
        "xmpp",
        to,
        message,
        0,
        newmessage,
        NULL,
        NULL);

    return err;
}

int
otrlib_decrypt_message(OtrlUserState user_state, OtrlMessageAppOps *ops, char *jid, const char * const from,
    const char * const message, char **decrypted, OtrlTLV **tlvs)
{
    return otrl_message_receiving(
        user_state,
        ops,
        NULL,
        jid,
        "xmpp",
        from,
        message,
        decrypted,
        tlvs,
        NULL,
        NULL);
}

void
otrlib_handle_tlvs(OtrlUserState user_state, OtrlMessageAppOps *ops, ConnContext *context, OtrlTLV *tlvs, GHashTable *smp_initiators)
{
    NextExpectedSMP nextMsg = context->smstate->nextExpected;
    OtrlTLV *tlv = otrl_tlv_find(tlvs, OTRL_TLV_SMP1);
    if (tlv) {
        if (nextMsg != OTRL_SMP_EXPECT1) {
            otrl_message_abort_smp(user_state, ops, NULL, context);
        } else {
            // [get secret from user and continue SMP];
            cons_debug("%s initiated SMP with secret", context->username);
            g_hash_table_insert(smp_initiators, strdup(context->username), strdup(context->username));
        }
    }
    tlv = otrl_tlv_find(tlvs, OTRL_TLV_SMP2);
    if (tlv) {
        if (nextMsg != OTRL_SMP_EXPECT2) {
            otrl_message_abort_smp(user_state, ops, NULL, context);
        } else {
            // If we received TLV2, we will send TLV3 and expect TLV4
            context->smstate->nextExpected = OTRL_SMP_EXPECT4;
        }
    }
    tlv = otrl_tlv_find(tlvs, OTRL_TLV_SMP3);
    if (tlv) {
        if (nextMsg != OTRL_SMP_EXPECT3) {
            otrl_message_abort_smp(user_state, ops, NULL, context);
        } else {
            // If we received TLV3, we will send TLV4
            // We will not expect more messages, so prepare for next SMP
            context->smstate->nextExpected = OTRL_SMP_EXPECT1;
            // Report result to user
            if ((context->active_fingerprint->trust != NULL) && (context->active_fingerprint->trust[0] != '\0')) {
                cons_debug("SMP SUCCESSFUL");
            } else {
                cons_debug("SMP UNSUCCESSFUL");
            }
        }
    }
    tlv = otrl_tlv_find(tlvs, OTRL_TLV_SMP4);
    if (tlv) {
        if (nextMsg != OTRL_SMP_EXPECT4) {
            otrl_message_abort_smp(user_state, ops, NULL, context);
        } else {
            // We will not expect more messages, so prepare for next SMP
            context->smstate->nextExpected = OTRL_SMP_EXPECT1;
            // Report result to user
            if ((context->active_fingerprint->trust != NULL) && (context->active_fingerprint->trust[0] != '\0')) {
                cons_debug("SMP SUCCESSFUL");
            } else {
                cons_debug("SMP UNSUCCESSFUL");
            }
        }
    }
    tlv = otrl_tlv_find(tlvs, OTRL_TLV_SMP_ABORT);
    if (tlv) {
        // The message we are waiting for will not arrive, so reset
        // and prepare for the next SMP
        context->smstate->nextExpected = OTRL_SMP_EXPECT1;
        cons_debug("SMP ABORTED");
    }
}