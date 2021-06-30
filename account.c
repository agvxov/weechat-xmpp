// This Source Code Form is subject to the terms of the Mozilla Public
// License, version 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <strophe.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <weechat/weechat-plugin.h>

#include "plugin.h"
#include "config.h"
#include "input.h"
#include "account.h"
#include "connection.h"
//#include "xmpp-api.h"
//#include "xmpp-request.h"
#include "user.h"
#include "channel.h"
#include "buffer.h"

struct t_account *accounts = NULL;
struct t_account *last_account = NULL;

char *account_options[ACCOUNT_NUM_OPTIONS][2] =
{ { "jid", "" },
  { "password", "" },
  { "tls", "normal" },
  { "nickname", "" },
  { "autoconnect", "" },
};

struct t_account *account__search(const char *name)
{
    struct t_account *ptr_account;

    if (!name)
        return NULL;

    for (ptr_account = accounts; ptr_account;
         ptr_account = ptr_account->next_account)
    {
        if (strcmp(ptr_account->name, name) == 0)
            return ptr_account;
    }

    /* account not found */
    return NULL;
}

struct t_account *account__casesearch (const char *name)
{
    struct t_account *ptr_account;

    if (!name)
        return NULL;

    for (ptr_account = accounts; ptr_account;
         ptr_account = ptr_account->next_account)
    {
        if (weechat_strcasecmp (ptr_account->name, name) == 0)
            return ptr_account;
    }

    /* account not found */
    return NULL;
}

int account__search_option(const char *option_name)
{
    int i;

    if (!option_name)
        return -1;

    for (i = 0; i < ACCOUNT_NUM_OPTIONS; i++)
    {
        if (weechat_strcasecmp(account_options[i][0], option_name) == 0)
            return i;
    }

    /* account option not found */
    return -1;
}

void account__log_emit_weechat(void *const userdata, const xmpp_log_level_t level,
                               const char *const area, const char *const msg)
{
    struct t_account *account = (struct t_account*)userdata;

    static const char *log_level_name[4] = {"debug", "info", "warn", "error"};

    time_t date = time(NULL);
    const char *timestamp = weechat_util_get_time_string(&date);

    weechat_printf(
        account ? account->buffer : NULL,
        _("%s%s (%s): %s"),
        weechat_prefix("error"), area,
        log_level_name[level], msg);
}

struct t_account *account__alloc(const char *name)
{
    struct t_account *new_account;
    int i, length;
    char *option_name;

    if (account__casesearch(name))
        return NULL;

    /* alloc memory for new account */
    new_account = malloc(sizeof(*new_account));
    if (!new_account)
    {
        weechat_printf(NULL,
                       _("%s%s: error when allocating new account"),
                       weechat_prefix("error"), WEECHAT_XMPP_PLUGIN_NAME);
        return NULL;
    }

    /* add new account to queue */
    new_account->prev_account = last_account;
    new_account->next_account = NULL;
    if (last_account)
        last_account->next_account = new_account;
    else
        accounts = new_account;
    last_account = new_account;

    /* set name */
    new_account->name = strdup(name);

    /* internal vars */
    new_account->reloading_from_config = 0;

    new_account->is_connected = 0;
    new_account->disconnected = 0;

    new_account->logger.handler = &account__log_emit_weechat;
    new_account->logger.userdata = new_account;
    new_account->context = xmpp_ctx_new(NULL, &new_account->logger);
    new_account->connection = NULL;

    new_account->nickname = NULL;

    new_account->buffer = NULL;
    new_account->buffer_as_string = NULL;
    new_account->users = NULL;
    new_account->last_user = NULL;
    new_account->channels = NULL;
    new_account->last_channel = NULL;

    /* create options with null value */
    for (i = 0; i < ACCOUNT_NUM_OPTIONS; i++)
    {
        new_account->options[i] = NULL;

        length = strlen(new_account->name) + 1 +
            strlen(account_options[i][0]) +
            512 +  /* inherited option name(xmpp.account_default.xxx) */
            1;
        option_name = malloc(length);
        if (option_name)
        {
            snprintf(option_name, length, "%s.%s << xmpp.account_default.%s",
                     new_account->name,
                     account_options[i][0],
                     account_options[i][0]);
            new_account->options[i] = config__account_new_option(
                config_file,
                config_section_account,
                i,
                option_name,
                account_options[i][1],
                account_options[i][1],
                0,
                &config__account_check_value_cb,
                account_options[i][0],
                NULL,
                &config__account_change_cb,
                account_options[i][0],
                NULL);
            config__account_change_cb(account_options[i][0], NULL,
                                      new_account->options[i]);
            free(option_name);
        }
    }

    return new_account;
}

void account__free_data(struct t_account *account)
{
    int i;

    if (!account)
        return;

    /* free linked lists */
    /*
    for (i = 0; i < IRC_SERVER_NUM_OUTQUEUES_PRIO; i++)
    {
        account__outqueue_free_all(account, i);
    }
    xmpp_redirect_free_all(account);
    xmpp_notify_free_all(account);
    */
    channel__free_all(account);
    user__free_all(account);

    /* free hashtables */
    /*
    weechat_hashtable_free(account->join_manual);
    weechat_hashtable_free(account->join_channel_key);
    weechat_hashtable_free(account->join_noswitch);
    */

    /* close xmpp context */
    if (account->connection)
        xmpp_conn_release(account->connection);
    if (account->context)
        xmpp_ctx_free(account->context);

    /* free account data */
  //for (i = 0; i < ACCOUNT_NUM_OPTIONS; i++)
  //{
  //    if (account->options[i])
  //        weechat_config_option_free(account->options[i]);
  //}

    if (account->name)
        free(account->name);

    if (account->buffer_as_string)
        free(account->buffer_as_string);

  //channel__free_all(account);
  //user__free_all(account);
}

void account__free(struct t_account *account)
{
    struct t_account *new_accounts;

    if (!account)
        return;

    /*
     * close account buffer (and all channels/privates)
     * (only if we are not in a /upgrade, because during upgrade we want to
     * keep connections and closing account buffer would disconnect from account)
     */
    if (account->buffer)
        weechat_buffer_close(account->buffer);

    /* remove account from queue */
    if (last_account == account)
        last_account = account->prev_account;
    if (account->prev_account)
    {
        (account->prev_account)->next_account = account->next_account;
        new_accounts = accounts;
    }
    else
        new_accounts = account->next_account;

    if (account->next_account)
        (account->next_account)->prev_account = account->prev_account;

    account__free_data(account);
    free(account);
    accounts = new_accounts;
}

void account__free_all()
{
    /* for each account in memory, remove it */
    while (accounts)
    {
        account__free(accounts);
    }
}

void account__disconnect(struct t_account *account, int reconnect)
{
    (void) reconnect;

    struct t_channel *ptr_channel;
        (void) ptr_channel;

    if (account->is_connected)
    {
        /*
         * remove all nicks and write disconnection message on each
         * channel/private buffer
         */
        user__free_all(account);
        weechat_nicklist_remove_all(account->buffer);
        for (ptr_channel = account->channels; ptr_channel;
             ptr_channel = ptr_channel->next_channel)
        {
            weechat_nicklist_remove_all(ptr_channel->buffer);
            weechat_printf(
                ptr_channel->buffer,
                _("%s%s: disconnected from account"),
                weechat_prefix("network"), WEECHAT_XMPP_PLUGIN_NAME);
        }
        /* remove away status on account buffer */
        //weechat_buffer_set(account->buffer, "localvar_del_away", "");
    }

    account__close_connection(account);

    if (account->buffer)
    {
        weechat_printf(
            account->buffer,
            _("%s%s: disconnected from account"),
            weechat_prefix ("network"), WEECHAT_XMPP_PLUGIN_NAME);
    }

    /*
    account->current_retry = 0;

    if (switch_address)
        account__switch_address(account, 0);
    else
        account__set_index_current_address(account, 0);

    if (account->nick_modes)
    {
        free (account->nick_modes);
        account->nick_modes = NULL;
        weechat_bar_item_update ("input_prompt");
        weechat_bar_item_update ("xmpp_nick_modes");
    }
    account->cap_away_notify = 0;
    account->cap_account_notify = 0;
    account->cap_extended_join = 0;
    account->is_away = 0;
    account->away_time = 0;
    account->lag = 0;
    account->lag_displayed = -1;
    account->lag_check_time.tv_sec = 0;
    account->lag_check_time.tv_usec = 0;
    account->lag_next_check = time (NULL) +
        weechat_config_integer (xmpp_config_network_lag_check);
    account->lag_last_refresh = 0;
    account__set_lag (account);
    account->monitor = 0;
    account->monitor_time = 0;

    if (reconnect
        && IRC_SERVER_OPTION_BOOLEAN(account, IRC_SERVER_OPTION_AUTORECONNECT))
        account__reconnect_schedule(account);
    else
    {
        account->reconnect_delay = 0;
        account->reconnect_start = 0;
    }
        */

    /* discard current nick if no reconnection asked */
        /*
    if (!reconnect && account->nick)
        account__set_nick(account, NULL);

    account__set_buffer_title(account);

    account->disconnected = 1;
        */

    /* send signal "account_disconnected" with account name */
        /*
    (void) weechat_hook_signal_send("account_disconnected",
                                    WEECHAT_HOOK_SIGNAL_STRING, account->name);
        */
}

void account__disconnect_all()
{
    struct t_account *ptr_account;

    for (ptr_account = accounts; ptr_account;
         ptr_account = ptr_account->next_account)
    {
        account__disconnect(ptr_account, 0);
    }
}

struct t_gui_buffer *account__create_buffer(struct t_account *account)
{
    char buffer_name[256], charset_modifier[256];

    snprintf(buffer_name, sizeof(buffer_name),
             "account.%s", account->name);
    account->buffer = weechat_buffer_new(buffer_name,
                                         &input__data_cb, NULL, NULL,
                                         &buffer__close_cb, NULL, NULL);
    if (!account->buffer)
        return NULL;

    if (!weechat_buffer_get_integer(account->buffer, "short_name_is_set"))
        weechat_buffer_set(account->buffer, "short_name", account->name);
    weechat_buffer_set(account->buffer, "localvar_set_type", "server");
    weechat_buffer_set(account->buffer, "localvar_set_server", account->name);
    weechat_buffer_set(account->buffer, "localvar_set_channel", account->name);
    snprintf(charset_modifier, sizeof (charset_modifier),
             "account.%s", account->name);
    weechat_buffer_set(account->buffer, "localvar_set_charset_modifier",
                       charset_modifier);
    weechat_buffer_set(account->buffer, "title",
                       (account->name) ? account->name : "");

    weechat_buffer_set(account->buffer, "nicklist", "1");
    weechat_buffer_set(account->buffer, "nicklist_display_groups", "0");
    weechat_buffer_set_pointer(account->buffer, "nicklist_callback",
                               &buffer__nickcmp_cb);
    weechat_buffer_set_pointer(account->buffer, "nicklist_callback_pointer",
                               account);

    return account->buffer;
}

void account__close_connection(struct t_account *account)
{
    if (account->connection)
    {
        if (xmpp_conn_is_connected(account->connection))
            xmpp_disconnect(account->connection);
    }

    account->is_connected = 0;
}

int account__connect(struct t_account *account)
{
    account->disconnected = 0;

    if (!account->buffer)
    {
        if (!account__create_buffer(account))
            return 0;
        weechat_buffer_set(account->buffer, "display", "auto");
    }

    account__close_connection(account);

    account->is_connected =
        connection__connect(account, &account->connection,
                            weechat_config_string(account->options[ACCOUNT_OPTION_JID]),
                            weechat_config_string(account->options[ACCOUNT_OPTION_PASSWORD]),
                            weechat_config_string(account->options[ACCOUNT_OPTION_TLS]));

    return account->is_connected;
}

int account__timer_cb(const void *pointer, void *data, int remaining_calls)
{
    (void) pointer;
    (void) data;
    (void) remaining_calls;

    struct t_account *ptr_account;

    for (ptr_account = accounts; ptr_account;
         ptr_account = ptr_account->next_account)
    {
        if (ptr_account->is_connected
            && (xmpp_conn_is_connecting(ptr_account->connection)
                || xmpp_conn_is_connected(ptr_account->connection)))
            connection__process(ptr_account->context, ptr_account->connection, 10);
    }
}