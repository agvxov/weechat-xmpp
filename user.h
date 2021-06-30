// This Source Code Form is subject to the terms of the Mozilla Public
// License, version 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef _WEECHAT_XMPP_USER_H_
#define _WEECHAT_XMPP_USER_H_

struct t_user_profile
{
    char *avatar_hash;
    char *status_text;
    char *status_emoji;
    char *real_name;
    char *display_name;
    char *real_name_normalized;
    char *email;
    char *team;
    char *bot_id;
};

struct t_user
{
    char *id;
    char *name;
    char *team_id;
    char *real_name;
    char *colour;

    int deleted;
    char *tz;
    char *tz_label;
    int tz_offset;
    char *locale;

    struct t_user_profile profile;
    int updated;
    int is_away;

    int is_admin;
    int is_owner;
    int is_primary_owner;
    int is_restricted;
    int is_ultra_restricted;
    int is_bot;
    int is_stranger;
    int is_app_user;
    int has_2fa;

    struct t_user *prev_user;
    struct t_user *next_user;
};
struct t_channel;

const char *user__get_colour(struct t_user *user);

const char *user__as_prefix(struct t_account *account,
                                 struct t_user *user,
                                 const char *name);

struct t_user *user__bot_search(struct t_account *account,
                                           const char *bot_id);

struct t_user *user__search(struct t_account *account,
                                       const char *id);

struct t_user *user__new(struct t_account *account,
                                    const char *id, const char *display_name);

void user__free_all(struct t_account *account);

void user__nicklist_add(struct t_account *account,
                             struct t_channel *channel,
                             struct t_user *user);

#endif /*WEECHAT_XMPP_USER_H*/