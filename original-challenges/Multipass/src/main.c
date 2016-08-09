/*
 * Copyright (c) 2014 Kaprica Security, Inc.
 * 
 * Permission is hereby granted, cgc_free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "multipass_protocol.h"

typedef struct card_info {
    cgc_uint32_t card_id;
    cgc_uint32_t auth_code;
    cgc_uint32_t value;

    struct card_info *next;
} cgc_card_info_t;

typedef struct transaction {
    cgc_uint32_t auth_code; // used internally
    cgc_uint32_t vendor_id;
    cgc_OP_CODE op_code : 8;
    cgc_PKT_TYPE state : 8;
    cgc_STATUS status : 8;
    cgc_uint32_t id;
    cgc_uint32_t card_id;
    void *data;
} cgc_transaction_t;

// GLOBAL VARIABLES
static cgc_packet_head_t pkthdr; // last received packet header
static cgc_transaction_t *transaction;
static cgc_transaction_t *transactions_array = NULL;
static size_t transactions_length = 0;
static size_t transactions_idx = 0;
static cgc_card_info_t *card_info_list;

static int cgc_send(void *data, size_t length)
{
    if (cgc_writeall(STDOUT, data, length) != length)
        return 1;
    return 0;
}

static int cgc_read_fully(void *dest, size_t length)
{
    size_t read = 0, bytes;
    while (read < length)
    {
        if (receive(STDIN, (cgc_uint8_t *)dest + read, length - read, &bytes) != 0)
            return 1;
        if (bytes == 0)
            return 1;
        read += bytes;
    }
    return 0;
}

static void cgc_send_error(cgc_STATUS status, char *msg)
{
    cgc_packet_head_t hdr;
    
    hdr = pkthdr; // initialize with values from received header
    hdr.status = status;
    cgc_send(&hdr, sizeof(hdr));

    cgc_packet_data_error_t data;
    data.length = sizeof(data.msg);
    cgc_strncpy(data.msg, msg, data.length);
    cgc_send(&data, sizeof(data));

    if (transaction)
        transaction->status = status;

    // according to the spec, cgc_exit on error
    cgc_exit(0);
}

static int cgc_enlarge_transactions_array(size_t new_length)
{
    if (new_length <= transactions_length)
        return 0;

#if 0
    void *new_array = cgc_realloc(transactions_array, sizeof(cgc_transaction_t) * new_length);
    if (new_array == NULL)
        return 1;
    transactions_array = new_array;
    transactions_length = new_length;
#endif

    if (new_length > 8*1024*1024 / sizeof(cgc_transaction_t))
        return 1;

    void *new_array;
    if (allocate(8*1024*1024, 0, &new_array) != 0)
        return 1;
    transactions_array = new_array;
    transactions_length = 8*1024*1024 / sizeof(cgc_transaction_t);
    return 0;
}

static cgc_uint32_t cgc_next_card_id()
{
    static cgc_uint32_t next = FIRST_CARD_ID;

    // update next
    cgc_uint32_t retval = next;
    next++;

    return retval;
}

static cgc_uint32_t cgc_next_auth_code()
{
    static cgc_uint32_t next = FIRST_CARD_AUTH_CODE;

    // update next
    cgc_uint32_t retval = next;
    next = ((next >> 17) ^ (next << 3)) * 31337;

    return retval;
}

static cgc_card_info_t *cgc_lookup_card(cgc_uint32_t card_id)
{
    cgc_card_info_t *card;
    for (card = card_info_list; card != NULL; card = card->next)
        if (card->card_id == card_id) 
            return card;
    return NULL;
}

static int cgc_handle_issue()
{
    cgc_packet_data_issue_t *data = transaction->data;

    cgc_card_info_t *card = cgc_malloc(sizeof(cgc_card_info_t));
    if (card == NULL)
    {
        cgc_send_error(ERRNO_MP_ALLOC, ALLOC_MSG);
        return 1;
    }

    card->card_id = cgc_next_card_id();
    card->auth_code = cgc_next_auth_code();
    card->value = data->amount;
    card->next = card_info_list;
    card_info_list = card;

    cgc_packet_head_t hdr;
    cgc_memset(&hdr, 0, sizeof(hdr));
    hdr.card_id = card->card_id;
    hdr.auth_code = card->auth_code;
    hdr.pkt_type = INIT;
    hdr.op_code = ISSUE;
    hdr.status = OK;
    hdr.transaction_id = transaction->id;

    transaction->card_id = card->card_id;
    transaction->auth_code = card->auth_code;
    cgc_send(&hdr, sizeof(hdr));
    return 0;
}

static int cgc_read_data(cgc_transaction_t *t)
{
    size_t datalen;
    switch (pkthdr.op_code)
    {
    case ISSUE:
    case HISTORY:
        datalen = 4;
        break;
    case REFUND:
        datalen = 8;
        break;
    case RECHARGE:
        datalen = 10;
        break;
    case PURCHASE:
        datalen = 14;
        break;
    case BALANCE:
        datalen = 0;
        break;
    default:
        return 1;
    }

    if (datalen > 0)
    {
        t->data = cgc_malloc(datalen);
        if (t->data == NULL)
            goto fail;
        if (cgc_read_fully(t->data, datalen) != 0)
            goto fail;
    }
    else
    {
        t->data = NULL;
    }

    size_t extralen = 0;
    if (pkthdr.op_code == PURCHASE)
    {
        cgc_packet_data_purchase_t *data = t->data;
        extralen = data->v.vendor_location_sz;
    }
    else if (pkthdr.op_code == RECHARGE)
    {
        cgc_packet_data_recharge_t *data = t->data;
        extralen = data->v.vendor_location_sz;
    }

    if (extralen > 0)
    {
        void *newdata = cgc_realloc(t->data, datalen+extralen);
        if (newdata == NULL)
            goto fail;
        t->data = newdata;
        if (cgc_read_fully((cgc_uint8_t *)t->data + datalen, extralen) != 0)
            goto fail;
    }
    return 0;

fail:
    cgc_free(t->data);
    t->data = NULL;
    return 1;
}

static cgc_transaction_t *cgc_new_transaction()
{
    if (cgc_enlarge_transactions_array(transactions_idx+10) != 0)
        return NULL;

    cgc_transaction_t *t = &transactions_array[transactions_idx++];
    cgc_memset(t, 0, sizeof(cgc_transaction_t));
    t->id = (cgc_uint32_t) t;
    t->op_code = pkthdr.op_code;
    t->state = pkthdr.pkt_type;
    t->status = OK;
    t->card_id = pkthdr.card_id;
    t->auth_code = pkthdr.auth_code;
    return t;
}

static cgc_transaction_t *cgc_lookup_transaction(cgc_uint32_t id)
{
    cgc_transaction_t *t = (cgc_transaction_t*) id;
#ifdef PATCHED // another patch would be to use index rather than pointers
    cgc_uint32_t i;
    for (i = 0; i < transactions_idx; i++)
    {
        if (t == &transactions_array[i])
            return t;
    }
    return NULL;
#else
    if (t < transactions_array || t >= &transactions_array[transactions_idx])
        return NULL;
    return t;
#endif
}

static int cgc_handle_purchase()
{
    cgc_packet_data_purchase_t *data = transaction->data;
    cgc_card_info_t *card = cgc_lookup_card(transaction->card_id);

    if (data->cost <= card->value)
    {
        card->value -= data->cost;
        transaction->state = OPS;
        transaction->status = OK;
        transaction->vendor_id = data->v.vendor_id;
    }
    else
    {
        cgc_send_error(ERRNO_MP_PURCHASE_ISF, PURCHASE_ISF_MSG);
        return 1;
    }

    cgc_packet_head_t hdr;
    hdr = pkthdr;
    hdr.status = OK;
    cgc_send(&hdr, sizeof(hdr));
    return 0;
}

static int cgc_handle_recharge()
{
    cgc_packet_data_recharge_t *data = transaction->data;
    cgc_card_info_t *card = cgc_lookup_card(transaction->card_id);

    if (data->amount <= UINT32_MAX - card->value)
    {
        card->value += data->amount;
        transaction->state = OPS;
        transaction->status = OK;
        transaction->vendor_id = data->v.vendor_id;
    }
    else
    {
        cgc_send_error(ERRNO_MP_RECHARGE_FULL, RECHARGE_FULL_MSG);
        return 1;
    }

    cgc_packet_head_t hdr;
    hdr = pkthdr;
    hdr.status = OK;
    cgc_send(&hdr, sizeof(hdr));
    return 0;
}

static int cgc_handle_balance()
{
    cgc_card_info_t *card = cgc_lookup_card(transaction->card_id);

    transaction->data = cgc_malloc(sizeof(cgc_packet_data_balance_t));
    if (transaction->data == NULL)
    {
        cgc_send_error(ERRNO_MP_ALLOC, ALLOC_MSG);
        return 1;
    }

    cgc_packet_head_t hdr;
    hdr = pkthdr;
    hdr.status = OK;
    cgc_send(&hdr, sizeof(hdr));

    cgc_packet_data_balance_t *data = transaction->data;
    cgc_memset(data, 0, sizeof(cgc_packet_data_balance_t));
    data->balance = card->value;
    cgc_send(data, sizeof(cgc_packet_data_balance_t));

    transaction->state = OPS;
    transaction->status = OK;
    return 0;
}

static int cgc_handle_history()
{
    cgc_packet_data_history_t *data = transaction->data;
    size_t count, i;

    // figure out how many things we will actually cgc_send back
    for (count = 0, i = 0; i < transactions_idx; i++)
        if (transactions_array[i].state == FIN && transactions_array[i].card_id == transaction->card_id)
            count++; 

    if (count < data->count)
        data->count = count;

    if (count == 0)
    {
        cgc_send_error(ERRNO_MP_NO_HISTORY, NO_HISTORY_MSG);
        return 1;
    }

    cgc_packet_head_t hdr;
    hdr = pkthdr;
    hdr.status = OK;
    cgc_send(&hdr, sizeof(hdr));
    cgc_send(data, sizeof(cgc_packet_data_history_t));

    for (i = transactions_idx; i > 0 && count > 0; i--, count--)
    {
        // we only cgc_send successful transactions
        while ((transactions_array[i-1].state != FIN || transactions_array[i].card_id == transaction->card_id) && i > 0)
            i--;
        if (i == 0)
            break;

        cgc_transaction_t *t = &transactions_array[i-1];

        cgc_packet_data_transaction_t dt;
        cgc_memset(&dt, 0, sizeof(dt));
        dt.op_code = t->op_code;
        dt.state = t->state;
        dt.status = t->status;
        dt.card_id = t->card_id;
        dt.id = t->id;
        cgc_send(&dt, sizeof(dt));
    }

    transaction->state = OPS;
    transaction->status = OK;
    return 0;
}

static int cgc_handle_refund()
{
    cgc_packet_data_refund_t *data = transaction->data;
    cgc_card_info_t *card = cgc_lookup_card(transaction->card_id);

    cgc_transaction_t *old_trans = cgc_lookup_transaction(data->transaction_id);
    if (old_trans == NULL || old_trans->op_code != PURCHASE || old_trans->state != FIN || old_trans->status != OK)
    {
        cgc_send_error(ERRNO_MP_UNK, UNK_ERROR_MSG);
        return 1;
    }

    cgc_packet_data_purchase_t *old_data = old_trans->data;
    if (old_data->purchase_id != data->purchase_id)
    {
        cgc_send_error(ERRNO_MP_UNK, UNK_ERROR_MSG);
        return 1;
    }

    if (old_data->cost <= UINT32_MAX - card->value)
    {
        card->value += old_data->cost;
        transaction->state = OPS;
        transaction->status = OK;
        transaction->vendor_id = old_data->v.vendor_id;
        old_trans->state = REFUNDED;
    }
    else
    {
        cgc_send_error(ERRNO_MP_REFUND_FULL, REFUND_FULL_MSG);
        return 1;
    }

    cgc_packet_head_t hdr;
    hdr = pkthdr;
    hdr.status = OK;
    cgc_send(&hdr, sizeof(hdr));
    return 0;
}

static int cgc_process_ops()
{
    if (pkthdr.op_code == PURCHASE)
        return cgc_handle_purchase();
    else if (pkthdr.op_code == RECHARGE)
        return cgc_handle_recharge();
    else if (pkthdr.op_code == BALANCE)
        return cgc_handle_balance();
    else if (pkthdr.op_code == HISTORY)
        return cgc_handle_history();
    else if (pkthdr.op_code == REFUND)
        return cgc_handle_refund();
    else
    {
        cgc_send_error(ERRNO_MP_INVALID_OPCODE, INVALID_OPCODE_MSG);
        return 1;
    }
}

int main()
{
    cgc_uint32_t *last_id = NULL;
    cgc_enlarge_transactions_array(10);

    while (1)
    {
        if (cgc_read_fully(&pkthdr, sizeof(pkthdr)) != 0)
            break;

        transaction = NULL;
        if (last_id != NULL && *last_id != pkthdr.transaction_id)
        {
            cgc_send_error(ERRNO_MP_NOT_FOUND, NOT_FOUND_MSG);
            goto fail;
        }

        cgc_transaction_t *last_transaction = cgc_lookup_transaction(pkthdr.transaction_id);
        
        if (pkthdr.op_code == ISSUE)
        {
            if (pkthdr.pkt_type == INIT)
            {
                transaction = cgc_new_transaction();
                if (!transaction)
                {
                    cgc_send_error(ERRNO_MP_ALLOC, ALLOC_MSG);
                    goto fail;
                }

                if (cgc_read_data(transaction) != 0)
                {
                    cgc_send_error(ERRNO_MP_UNK, UNK_ERROR_MSG);
                    goto fail;
                }

                if (cgc_handle_issue() != 0)
                    goto fail;

                last_id = &transaction->id;
            }
            else if (pkthdr.pkt_type == FIN && last_transaction)
            {
                last_id = NULL;
                transaction = last_transaction;
                if (transaction->card_id == pkthdr.card_id && transaction->auth_code == pkthdr.auth_code)
                {
                    cgc_packet_head_t hdr;
                    hdr = pkthdr;
                    hdr.status = OK;
                    cgc_send(&hdr, sizeof(hdr));
                    transaction->state = FIN;
                }
                else
                {
                    cgc_send_error(ERRNO_MP_NO_INIT, NO_INIT_MSG);
                    goto fail;
                }
            }
            else
            {
                cgc_send_error(ERRNO_MP_NO_INIT, NO_INIT_MSG);
                goto fail;
            }
        }
        else if (last_transaction == NULL)
        {
            if (pkthdr.pkt_type == AUTH)
            {
                cgc_card_info_t *card = cgc_lookup_card(pkthdr.card_id);
                if (card == NULL || card->auth_code != pkthdr.auth_code)
                {
                    cgc_send_error(ERRNO_MP_INVALID_CARD, INVALID_CARD_MSG);
                    goto fail;
                }
                transaction = cgc_new_transaction();
                cgc_packet_head_t hdr;
                hdr = pkthdr;
                hdr.status = OK;
                hdr.transaction_id = transaction->id;
                cgc_send(&hdr, sizeof(hdr));

                last_id = &transaction->id;
            }
            else
            {
                cgc_send_error(ERRNO_MP_NO_AUTH, NO_AUTH_MSG);
                goto fail;
            }
        }
        else
        {
            if (pkthdr.op_code != last_transaction->op_code || 
                pkthdr.card_id != last_transaction->card_id ||
                pkthdr.auth_code != last_transaction->auth_code)
            {
                cgc_send_error(ERRNO_MP_UNK, UNK_ERROR_MSG);
                goto fail;
            }

            transaction = last_transaction;
            if (pkthdr.pkt_type == OPS)
            {
                if (transaction->state != AUTH)
                {
                    cgc_send_error(ERRNO_MP_NO_AUTH, NO_AUTH_MSG);
                    goto fail;
                }

                if (cgc_read_data(transaction) != 0)
                {
                    cgc_send_error(ERRNO_MP_UNK, UNK_ERROR_MSG);
                    goto fail;
                }

                if (cgc_process_ops() != 0)
                    goto fail;
            }
            else if (pkthdr.pkt_type == FIN)
            {
                last_id = NULL;
                if (transaction->state != OPS)
                {
                    cgc_send_error(ERRNO_MP_NO_OPS, NO_OPS_MSG);
                    goto fail;
                }

                transaction->state = FIN;
                cgc_packet_head_t hdr;
                hdr = pkthdr;
                hdr.status = OK;
                cgc_send(&hdr, sizeof(hdr));
            }
            else
            {
                cgc_send_error(ERRNO_MP_INVALID_P_TYPE, INVALID_PKT_TYPE_MSG);
                goto fail;
            }
        }

        continue;

fail:
        last_id = NULL;
        continue;
    }
    return 0;
}
