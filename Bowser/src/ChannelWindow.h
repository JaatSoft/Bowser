/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Bowser's source code is provided without any warranty what-so-ever; users may
 * use the source code at their own risk. In no way may the developers who have
 * contributed to the code be held accountable for damages that arise from the use
 * or misuse of the source code.
 * 
 * Anyone is free to use the source code however they like as long as credit is
 * given to The Bowser Team (http://bowser.sourceforge.net) in both the
 * documentation for the program and the source code (if you choose to release it.)
 * Any derivitive work using this code must _not_ use the name Bowser or some sort
 * of variation thereof unless explicit permission is granted from The Bowser Team
 * in advance. In addition, commercial developers may not use this source code
 * without explicit permssion from The Bowser Team in advance.
 *
 */

#ifndef CHANNELWINDOW_H_
#define CHANNELWINDOW_H_

#include <String.h>

#include "ClientWindow.h"

class BScrollView;
class ServerWindow;
class NamesView;

class ChannelWindow : public ClientWindow
{
  public:
                            ChannelWindow (
                              const char *,
                              int32,
                              const char *,
                              const BMessenger &,
                              const char *);

                            ~ChannelWindow (void);

    virtual bool            QuitRequested (void);

    virtual void            MessageReceived (BMessage *);
    virtual void            Parser (const char *);
    virtual void            StateChange (BMessage *msg);
    virtual void            TabExpansion (void);

    bool                    RemoveUser (const char *);
    int                     FindPosition (const char *);
    void                    UpdateMode (char, char);
    void                    ModeEvent (BMessage *);

    static int              SortNames (const void *, const void *);
    
  private:

    BString                 chanMode,
                            chanLimit,
                            chanLimitOld,
                            chanKey,
                            chanKeyOld,
                            lastExpansion;

    rgb_color               ctcpReqColor,
                            ctcpRpyColor,
                            whoisColor,
                            errorColor,
                            quitColor,
                            joinColor,
                            noticeColor;

    int32                   userCount,
                            opsCount;

    friend class            ClientWindow;
    NamesView               *namesList;
    BScrollView             *namesScroll;
};

#endif
