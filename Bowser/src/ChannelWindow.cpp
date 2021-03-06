
//#include <Application.h>
#include <ScrollView.h>
#include <FilePanel.h>
//#include <StringView.h>


#include <stdio.h>
#include <stdlib.h>
//#include <ctype.h>

#include "VTextControl.h"
#include "Names.h"
#include "Bowser.h"
//#include "IRCView.h"
#include "StatusView.h"
//#include "Settings.h"
#include "StringManip.h"
#include "IgnoreWindow.h"
#include "ChannelWindow.h"

ChannelWindow::ChannelWindow (
	const char *id_,
	int32 sid_,
	const char *serverName_,
	const BMessenger &sMsgr_,
	const char *nick)

	: ClientWindow (
		id_,
		sid_,
		serverName_,
		sMsgr_,
		nick,
		BRect (110,110,730,420)),

		chanMode (""),
		chanLimit (""),
		chanLimitOld (""),
		chanKey (""),
		chanKeyOld (""),
		lastExpansion (""),
		userCount (0),
		opsCount (0)

{
	SetSizeLimits (300,2000,150,2000);

	scroll->ResizeTo (
		scroll->Frame().Width() - 100,
		scroll->Frame().Height());

	BRect frame (bgView->Bounds());
	frame.left   = scroll->Frame().right + 1;
	frame.right -= B_V_SCROLL_BAR_WIDTH;
	frame.bottom = scroll->Frame().bottom - 1;

	namesList = new NamesView(frame);

	namesScroll = new BScrollView(
		"scroll_names",
		namesList,
		B_FOLLOW_RIGHT | B_FOLLOW_TOP_BOTTOM,
		0,
		false,
		true,
		B_PLAIN_BORDER);

	status->AddItem (new StatusItem (
		serverName.String(), 0),
		true);
		
	status->AddItem (new StatusItem (
		"Lag: ",
		"",
		STATUS_ALIGN_LEFT),
		true);

	status->AddItem (new StatusItem (
		0,
		"",
		STATUS_ALIGN_LEFT),
		true);

	status->AddItem (new StatusItem (
		"Users: ", ""),
		true);

	status->AddItem (new StatusItem (
		"Ops: ", ""),
		true);

	status->AddItem (new StatusItem (
		"Modes: ",
		""),
		true);

	status->AddItem (new StatusItem (
		"", "", 
		STATUS_ALIGN_LEFT),
		true);
	status->SetItemValue (STATUS_LAG, "0.000");
	status->SetItemValue (STATUS_NICK, myNick.String());
		
	bgView->AddChild (namesScroll);

	rgb_color joinColor = bowser_app->GetColor (C_JOIN);

	Display ("*** Now talking in ", &joinColor);
	Display (id.String(), &joinColor);
	Display ("\n", &joinColor);

}

ChannelWindow::~ChannelWindow (void)
{
	BListItem *nameItem;

	while ((nameItem = namesList->LastItem()))
	{
		namesList->RemoveItem (nameItem);
		delete nameItem;
	}
}

bool
ChannelWindow::QuitRequested()
{
	//printf ("ChannelWindow::QuitRequested, %s\n", id.String());
	BMessage *msg (CurrentMessage());
	
	if (!msg->HasBool ("bowser:part")
	||   msg->FindBool ("bowser:part"))
	{
		BMessage send (M_SERVER_SEND);

		AddSend (&send, "PART ");
		AddSend (&send, id);
		AddSend (&send, endl);
	}

	return ClientWindow::QuitRequested();
}

void
ChannelWindow::MessageReceived (BMessage *msg)
{
	switch (msg->what)
	{
		case M_USER_QUIT:
		{
			const char *nick;

			msg->FindString ("nick", &nick);

			if (RemoveUser (nick))
			{
				BMessage display;

				if (msg->FindMessage ("display", &display) == B_NO_ERROR)
					ClientWindow::MessageReceived (&display);
			}

			break;
		}

		case M_USER_ADD:
		{
			const char *nick;
			bool ignore;
			int32 iStatus (STATUS_NORMAL_BIT);

			msg->FindString ("nick", &nick);
			msg->FindBool ("ignore", &ignore);

			if (ignore) iStatus |= STATUS_IGNORE_BIT;

			namesList->AddItem (new NameItem (nick, iStatus));
			namesList->SortItems (SortNames);

			++userCount;
			BString buffer;
			buffer << userCount;

			status->SetItemValue (STATUS_USERS, buffer.String());

			BMessage display;
			if (msg->FindMessage ("display", &display) == B_NO_ERROR)
				ClientWindow::MessageReceived (&display);

			break;
		}

		case M_CHANGE_NICK:
		{
			const char *oldNick, *newNick;
			NameItem *item;
			int32 thePos;

			msg->FindString ("oldnick", &oldNick);
			msg->FindString ("newnick", &newNick);
			
			//printf ("oldnick: %s\n", oldNick);
			//printf ("newnick: %s\n", newNick);

			//myNick = newNick;
			
			if ((thePos = FindPosition (oldNick)) < 0
			||  (item = (static_cast<NameItem *>(namesList->ItemAt (thePos)))) == 0)
				return;

			item->SetName (newNick);
			namesList->SortItems (SortNames);

			if (myNick.ICompare (oldNick) == 0)
				status->SetItemValue (STATUS_NICK, newNick);
			

			ClientWindow::MessageReceived (msg);
			break;
		}

		case M_IGNORE_COMMAND:
		{
			IgnoreItem *iItem;
			bool hit (false);
			const char *ignoreCmd;

			msg->FindPointer (
				"item",
				reinterpret_cast<void **>(&iItem));
			msg->FindString ("ignorecmd", &ignoreCmd);

			for (int32 i = 0; i < namesList->CountItems(); ++i)
			{
				NameItem *nItem ((NameItem *)namesList->ItemAt (i));

				if ((nItem->Status() & STATUS_IGNORE_BIT) == 0
				&&   iItem->IsMatch (nItem->Name().String()))
				{
					const char *expansions[2];
					BString buffer;
					BString nick (nItem->Name());

					nItem->SetStatus (nItem->Status() | STATUS_IGNORE_BIT);

					expansions[0] = nick.String();
					expansions[1] = iItem->Text();
					buffer = ExpandKeyed (
						ignoreCmd,
						"Ni",
						expansions);

					rgb_color color = namesList->GetColor (C_IGNORE);
					Display (buffer.String(), &color);
					hit = true;
				}

				if (hit) namesList->SortItems (SortNames);
			}
			break;
		}

		case M_IGNORED_PRIVMSG:
		{
			const char *nick;
			const char *address;
			const char *rule;
			int32 position;
			NameItem *item;

			msg->FindString ("nick", &nick);
			msg->FindString ("address", &address);
			msg->FindString ("rule", &rule);

			if ((position = FindPosition (nick)) >= 0
			&&  (item = (NameItem *)namesList->ItemAt (position)) != 0
			&&  (item->Status() & STATUS_IGNORE_BIT) == 0)
			{
				const char *expansions[2];
				BString buffer;
				
				expansions[0] = nick;
				expansions[1] = rule;

				buffer = ExpandKeyed (
					bowser_app->GetCommand (CMD_IGNORE).String(),
					"Ni",
					expansions);

				rgb_color color = namesList->GetColor (C_IGNORE);
				Display (buffer.String(), &color);

				item->SetStatus (item->Status() | STATUS_IGNORE_BIT);
				item->SetAddress (address);
				namesList->SortItems (SortNames);
			}

			break;
		}

		case M_IGNORE_REMOVE:
		{
			BList *ignores;
			bool nHit (false);
			const char *ignoreCmd;

			msg->FindPointer (
				"ignores",
				reinterpret_cast<void **>(&ignores));
			msg->FindString ("ignorecmd", &ignoreCmd);

			for (int32 i = 0; i < namesList->CountItems(); ++i)
			{
				NameItem *nItem ((NameItem *)namesList->ItemAt (i));

				if ((nItem->Status() & STATUS_IGNORE_BIT) != 0)
				{
					BString nAddress (nItem->Address()),
						nName (nItem->Name());
					const char *address (
						nAddress.Length() ? nAddress.String() : 0);
					bool hit (false);
					IgnoreItem *iItem (0);


					for (int32 j = 0; j < ignores->CountItems(); ++j)
					{
						iItem = (IgnoreItem *)ignores->ItemAt (j);

						if (iItem->IsMatch (nName.String(), address))
						{
							hit = true;
							break;
						}
					}

					if (!hit)
					{
						// Remove Ignore status
						nItem->SetStatus (nItem->Status() & ~STATUS_IGNORE_BIT);
						nHit = true;
						BString name (nItem->Name()), buffer;
						const char *expansions[1];

						expansions[0] = name.String();

						// TODO Add ExpandKeyed
						buffer = ExpandKeyed (
							ignoreCmd,
							"N",
							expansions);

						rgb_color color = namesList->GetColor (C_IGNORE);
						Display (buffer.String(), &color);
					}
				}

				if (nHit)
					namesList->SortItems (SortNames);
			}
			break;
		}

		case M_CHANNEL_NAMES:
		{
			bool hit (false);

			for (int32 i = 0; msg->HasString ("nick", i); ++i)
			{
				const char *nick;
				bool op, voice, ignored;

				msg->FindString ("nick", i, &nick);
				msg->FindBool ("op", i, &op);
				msg->FindBool ("voice", i, &voice);
				msg->FindBool ("ignored", i, &ignored);

				if (FindPosition (nick) < 0)
				{
					int32 iStatus (ignored ? STATUS_IGNORE_BIT : 0);

					if (op)
					{
						++nick;
						++opsCount;
						iStatus |= STATUS_OP_BIT;
					}

					else if (voice)
					{
						++nick;
						iStatus |= STATUS_VOICE_BIT;
					}
					else
						iStatus |= STATUS_NORMAL_BIT;

					userCount++;

					namesList->AddItem (new NameItem (nick, iStatus));
					hit = true;
				}
			}

			if (hit)
			{
				namesList->SortItems (SortNames);
				BString buffer;
				buffer << opsCount;
				status->SetItemValue (STATUS_OPS, buffer.String());

				buffer = "";
				buffer << userCount;
				status->SetItemValue (STATUS_USERS, buffer.String());
			}
			break;
		}

		case M_CHANNEL_TOPIC:
		{
			const char *theTopic;
			BString buffer;

			msg->FindString ("topic", &theTopic);
			
			if (bowser_app->GetStatusTopicState())
			{
				status->SetItemValue (STATUS_TOPIC, theTopic);
			}
			
			if(bowser_app->GetShowTopicState())
			{
			    //show topic
				buffer << id << " : " << theTopic;
				SetTitle (buffer.String());
			}
			else
			{
			    //don't show topic
			    buffer << id;
			    SetTitle (buffer.String());
			}
				

			BMessage display;
			if (msg->FindMessage ("display", &display) == B_NO_ERROR)
				ClientWindow::MessageReceived (&display);
			break;
		}

		case M_CHANNEL_GOT_KICKED:
		{
		    
			BMessage display; // "you were kicked"
			if (msg->FindMessage ("display", &display) == B_NO_ERROR)
			{
				ClientWindow::MessageReceived (&display);
			}
				
			const char *theChannel;
			msg->FindString ("channel", &theChannel);			    
			    
			BMessage display2; // "attempting auto rejoin"
			if (msg->FindMessage ("display2", &display2) == B_NO_ERROR)
			{
				ClientWindow::MessageReceived (&display2);
			}
				
			BMessage send (M_SERVER_SEND);	
			AddSend (&send, "JOIN ");
			AddSend (&send, theChannel);	
			if (chanKey != "")
			{
				AddSend (&send, " ");
				AddSend (&send, chanKey);
			}
			AddSend (&send, endl);
					
			break;			
		}		
		

		case M_CHANNEL_MODE:
			ModeEvent (msg);
			break;

		case M_CHANNEL_MODES:
		{
			const char *mode;
			const char *chan;
			const char *msgz;

			msg->FindString ("mode", &mode);
			msg->FindString ("chan", &chan);
			msg->FindString ("msgz", &msgz);
						
			if (id.ICompare (chan) == 0)
			{
				BString realMode (GetWord (mode, 1));
				int32 place (2);

				if (realMode.FindFirst ("l") >= 0)
					chanLimit = GetWord (mode, place++);

				if (realMode.FindFirst ("k") >= 0)
					chanKey = GetWord (mode, place++);

				chanMode = mode;
				status->SetItemValue (STATUS_MODES, chanMode.String());
			}
			BMessage dispMsg (M_DISPLAY);
			PackDisplay (&dispMsg, msgz, &opColor, 0, bowser_app->GetStampState());
			PostMessage(&dispMsg);
			break;
		}

		case POPUP_MODE:
		{
			const char *inaction;
			msg->FindString ("action", &inaction);			
			
			int32 i = 0,
			      count = 0,
			      index = 0;
			BString victims,
			        targetNick,
			        action (inaction),
			        modechar,
			        polarity;
			NameItem *myUser;
			
			/// action ///
			if (action.FindFirst("voice") >= 0)
			{
				modechar = "v";
			}
			else if (action.FindFirst("op") >= 0)
			{
				modechar = "o";
			}
			
			/// polarity ///
			if (action.FindFirst("de") >= 0)
			{
				polarity << " -";
			}
			else
			{
				polarity << " +";
			}
			
			
			/// iterate ///			
   			while ((i = namesList->CurrentSelection(index++)) >= 0)
   			{ 				
				myUser = static_cast<NameItem *>(namesList->ItemAt(i));
				targetNick = myUser->Name();
				
				victims << " " << targetNick;
				count++;
			}
			
					
			BString command ("/mode ");
			command << id << polarity;
			
			for(int32 i = 0; i < count; i++) {
				command << modechar;
			}
			
			command << victims;
			
			ParseCmd (command.String());
			
		    break;
		}

		case POPUP_CTCP:
		{
			const char *inaction;
			msg->FindString ("action", &inaction);			
			
			int32 i = 0,
			      index = 0;
			BString victims,
			        targetNick,
			        action (inaction);
			NameItem *myUser;
			action.ToUpper();
			
			/// iterate ///			
   			while ((i = namesList->CurrentSelection(index++)) >= 0)
   			{ 				
				myUser = static_cast<NameItem *>(namesList->ItemAt(i));
				targetNick = myUser->Name();
				
				victims << targetNick << ",";
			}
			
			victims.RemoveLast (",");
			
			BString command ("/ctcp ");
			command << victims << " " << action;
							
			ParseCmd (command.String());
			
		    break;
		}

		case POPUP_WHOIS:
		{
			int32 i = 0,
			      index = 0;
			BString victims,
					targetNick;
			NameItem *myUser;
			
			/// iterate ///			
   			while ((i = namesList->CurrentSelection(index++)) >= 0)
   			{ 				
				myUser = static_cast<NameItem *>(namesList->ItemAt(i));
				targetNick = myUser->Name();
				
				victims << targetNick << ",";
			}
			
			victims.RemoveLast (",");
			
			BString command ("/whois ");
			command << victims;
							
			ParseCmd (command.String());
			
		    break;
		}

		case POPUP_KICK:
		{
			int32 i = 0,
			      index = 0;
			BString targetNick,
			        kickMsg (bowser_app->GetCommand (CMD_KICK));
			NameItem *myUser;
			
			/// iterate ///			
   			while ((i = namesList->CurrentSelection(index++)) >= 0)
   			{ 				
				myUser = static_cast<NameItem *>(namesList->ItemAt(i));
				targetNick = myUser->Name();
				
				BString command ("/kick ");
				command << targetNick << " " << kickMsg;
							
				ParseCmd (command.String());
			}
			
		    break;
		}
		
		case SEND_ACTION: // DCC send
		{
			BString targetNick;
			NameItem *myUser;
			int32 pos = namesList->CurrentSelection();
			if(pos >= 0)
			{
				myUser = static_cast<NameItem *>(namesList->ItemAt(pos));
				targetNick = myUser->Name();
			}
			BFilePanel *myPanel = new BFilePanel;
			BString myTitle ("Sending a file to ");
			myTitle.Append (targetNick);
			myPanel->Window()->SetTitle (myTitle.String());
			BMessage *myMessage = new BMessage (CHOSE_FILE);
			myMessage->AddString ("nick", targetNick.String());
			myPanel->SetMessage (myMessage);
			myPanel->SetButtonLabel (B_DEFAULT_BUTTON, "Send");
			myPanel->SetTarget (sMsgr);
			myPanel->Show();
			break;
		}
		case CHAT_ACTION: // DCC chat
		{
			BString targetNick;
			NameItem *myUser;
			int32 pos = namesList->CurrentSelection();
			if (pos >= 0)
			{
				myUser = static_cast<NameItem *>(namesList->ItemAt(pos));
				targetNick = myUser->Name();
			}

			BMessage msg (CHAT_ACTION);

			msg.AddString ("nick", targetNick.String());
			sMsgr.SendMessage (&msg); // (send for access to chanarray)
			break;
		}
		case OPEN_MWINDOW:
		{
			const char *theNick;
			msg->FindString("nick", &theNick);
			if (theNick == NULL)
			{
				NameItem *myUser;
				int32 pos = namesList->CurrentSelection();
				if(pos >= 0)
				{
					myUser = static_cast<NameItem *>(namesList->ItemAt(pos));
					BString targetNick = myUser->Name();
					msg->AddString ("nick", targetNick.String());
				}
			}
			sMsgr.SendMessage (msg);
			break;
		}
		
		case M_LAG_CHANGED:
		{
			BString lag;
			msg->FindString ("lag", &lag);
			status->SetItemValue (STATUS_LAG, lag.String());
			break;
		}

		case M_REJOIN:
		{
			const char *newNick;
			msg->FindString ("nickname", &newNick);
			myNick = newNick;	// update nickname (might have changed
			                    //                  on reconnect)
						
			rgb_color errorColor = bowser_app->GetColor (C_ERROR);
			Display ("[@] Attempting to rejoin...\n", &errorColor, &serverFont);
		
			namesList->ClearList();  // cleanup
			opsCount = 0;
			userCount = 0;
		
			BMessage send (M_SERVER_SEND);	
			AddSend (&send, "JOIN ");
			AddSend (&send, id);	
			if (chanKey != "")
			{
				AddSend (&send, " ");
				AddSend (&send, chanKey);
			}
			AddSend (&send, endl);
			break;		
		}
				
		default:
			ClientWindow::MessageReceived (msg);
			break;
	}
}

void
ChannelWindow::Parser (const char *buffer)
{
	lastExpansion = "";
	BMessage send (M_SERVER_SEND);

	AddSend (&send, "PRIVMSG ");
	AddSend (&send, id);
	AddSend (&send, " :");
	AddSend (&send, buffer);
	AddSend (&send, endl);

	Display ("<", 0, 0, true);
	Display (myNick.String(), &myNickColor);
	Display ("> ", 0);

	BString sBuffer (buffer);

	int32 hit;

	do
	{
		int32 place (0);
		BString nick;

		hit = sBuffer.Length();

		for (int32 i = 0; i < namesList->CountItems(); ++i)
		{
			NameItem *item (static_cast<NameItem *>(namesList->ItemAt (i)));
			BString iNick (item->Name());

			if (myNick.ICompare (item->Name())
			&& (place = FirstSingleKnownAs (sBuffer, iNick)) != B_ERROR
			&&  place < hit)
			{
				hit = place;
				nick = item->Name();
				break;
			}
		}

		BString tempString;

		if (hit < sBuffer.Length())
		{
			if (hit)
			{
				sBuffer.MoveInto (tempString, 0, hit);
				Display (tempString.String(), 0);
			}

			sBuffer.MoveInto (tempString, 0, nick.Length());
			Display (tempString.String(), &nickColor);

			if (atoi (autoNickTime.String()) > 0)
				nickTimes.AddItem (new TimedNick (
					nick.String(),
					system_time()));
		}

	} while (hit < sBuffer.Length());

	if (sBuffer.Length()) Display (sBuffer.String(), 0);
	Display ("\n", 0);
}

bool
ChannelWindow::RemoveUser (const char *data)
{
	int32 myIndex (FindPosition (data));

	if (myIndex != -1)
	{
		NameItem *item;

		namesList->Deselect (myIndex);
		if ((item = (NameItem *)namesList->RemoveItem (myIndex)) != 0)
		{
			BString buffer;

			if ((item->Status() & STATUS_OP_BIT) != 0)
			{
				--opsCount;
				buffer << opsCount;
				status->SetItemValue (STATUS_OPS, buffer.String());

				buffer = "";
			}

			--userCount;
			buffer << userCount;
			status->SetItemValue (STATUS_USERS, buffer.String());

			delete item;
			return true;
		}
	}
	return false;
}

int
ChannelWindow::FindPosition (const char *data)
{
	int32 count (namesList->CountItems());

	for (int32 i = 0; i < count; ++i)
	{
		NameItem *item ((NameItem *)(namesList->ItemAt (i)));
		BString nick (item->Name());

		if ((nick[0] == '@' || nick[0] == '+')
		&&  *data != nick[0])
			nick.Remove (0, 1);

		if ((*data == '@' || *data == '+')
		&&   nick[0] != *data)
			++data;

		if (!nick.ICompare (data))
			return i;
	}

	return -1;
}

int
ChannelWindow::SortNames(const void *name1, const void *name2)
{
	NameItem **firstPtr = (NameItem **)name1;
	NameItem **secondPtr = (NameItem **)name2;

	// Not sure if this can happen, and we
	// are assuming that if one is NULL
	// we return them as equal.  What if one
	// is NULL, and the other isn't?
	if (!firstPtr
	||  !secondPtr
	||  !(*firstPtr)
	||  !(*secondPtr))
		return 0;

	BString first, second;

	first << (((*firstPtr)->Status() & STATUS_OP_BIT) ? STATUS_OP_BIT : (*firstPtr)->Status()); 
	second << (((*secondPtr)->Status() & STATUS_OP_BIT) ? STATUS_OP_BIT : (*secondPtr)->Status()); 
	first.Prepend ('0', 10 - first.Length());
	second.Prepend ('0', 10 - second.Length());

	first   << (*firstPtr)->Name();
	second  << (*secondPtr)->Name();

	return first.ICompare (second);
}

void
ChannelWindow::TabExpansion (void)
{
	int32 start, finish;
	static int32 lastindex;
	static BString lastNick;
	static BList myList;
	input->TextView()->GetSelection (&start, &finish);

	if (input->TextView()->TextLength()
	&&  start == finish
	&&  start == input->TextView()->TextLength())
	{
		const char *inputText (
			input->TextView()->Text()
			+ input->TextView()->TextLength());
		const char *place (inputText);
	
		while (place > input->TextView()->Text())
		{
			if (*(place - 1) == '\x20')
			break;
			--place;
		}
		
		if (lastExpansion == "" 
			|| lastExpansion.ICompare(place, strlen(lastExpansion.String())) != 0
			|| lastNick != place)
		{
			lastindex = 0;
			lastExpansion = place;
			if (!myList.IsEmpty())
				myList.MakeEmpty();
			int32 count (namesList->CountItems());
			for (int32 i = 0; i < count ; i++)
			{
				if (!((NameItem *)namesList->ItemAt(i))->Name().ICompare(lastExpansion.String(), strlen(lastExpansion.String())))
					myList.AddItem(namesList->ItemAt(i));
			}
		}
			// We first check if what the user typed matches the channel
			// If that doesn't match, we check the names
			BString insertion;
	
			if (!id.ICompare (place, strlen (place)))
				insertion = id;
			else
			{
				int32 count (myList.CountItems());
				if (count > 0)
				{
					insertion = ((NameItem *)myList.ItemAt(lastindex++))->Name();
					if (lastindex == count) lastindex = 0;
					lastNick = insertion;
				}
			}

		if (insertion.Length())
		{
			input->TextView()->Delete (
				place - input->TextView()->Text(),
				input->TextView()->TextLength());

			input->TextView()->Insert (insertion.String());
			input->TextView()->Select (
				input->TextView()->TextLength(),
				input->TextView()->TextLength());
		}
	}
}

void
ChannelWindow::UpdateMode(char theSign, char theMode)
{
	char modeString[2]; // necessary C-style string
	sprintf(modeString, "%c", theMode);
	if(theSign == '-')
	{
		if(theMode == 'l')
		{
			BString myTemp = chanLimit;
			myTemp.Append(" ");
			chanMode.RemoveLast(myTemp);
			myTemp = chanLimit;
			myTemp.Prepend(" ");
			chanMode.RemoveLast(myTemp);
			chanMode.RemoveLast(chanLimit);
		}
		else if(theMode == 'k')
		{
			BString myTemp = chanKey;
			myTemp.Append(" ");
			chanMode.RemoveLast(myTemp);
			myTemp = chanKey;
			myTemp.Prepend(" ");
			chanMode.RemoveLast(myTemp);
			chanMode.RemoveLast(chanKey);
		}
		chanMode.RemoveFirst(modeString);
	}
	else
	{
		BString theReal = GetWord(chanMode.String(), 1);
		BString theRest = RestOfString(chanMode.String(), 2);
		theReal.RemoveFirst(modeString);
		theReal.Append(modeString);
		BString tempString(theReal);
		if(theRest != "-9z99")
		{
			tempString << " " << theRest;
		}
		
		if(theMode == 'l')
		{
			if (chanLimitOld != "")
			{
				BString theOld(" ");
				theOld << chanLimitOld;
				tempString.RemoveFirst(theOld);
			}
			tempString.Append(" ");
			tempString.Append(chanLimit);
			chanLimitOld = chanLimit;
		}
		else if(theMode == 'k')
		{
			if (chanKeyOld != "")
			{
				BString theOld(" ");
				theOld << chanKeyOld;
				tempString.RemoveFirst(theOld);
			}
			tempString.Append(" ");
			tempString.Append(chanKey);
			chanKeyOld = chanKey;
		}
		chanMode = tempString;
	}

	status->SetItemValue (STATUS_MODES, chanMode.String());
}


void
ChannelWindow::ModeEvent (BMessage *msg)
{
	int32 modPos (0), targetPos (1);
	const char *mode (0);
	const char *target (0);
	const char *theNick (0);
	char theOperator (0);
	bool hit (false);
	bool timeStamp = bowser_app->GetStampState();
	

	// TODO Change Status to bitmask -- Too hard this way
	msg->FindString ("mode", &mode);
	msg->FindString ("target", &target);
	msg->FindString ("nick", &theNick);


	BString buffer,
	        targetS (target);
		
	buffer << "*** " << theNick << " set mode: " << mode;
	
	if (targetS != "-9z99")
	{
		buffer << " " << targetS;
	}
			
	buffer << "\n";

	BMessage modeMsg (M_DISPLAY);
	PackDisplay (&modeMsg, buffer.String(), &opColor, 0, timeStamp);
	PostMessage(&modeMsg);
	
	
	// at least one
	if (mode && *mode && *(mode + 1))
	{
		theOperator = mode[modPos++];
	}

	while (theOperator && mode[modPos])
	{
		char theModifier (mode[modPos]);

		if (theModifier == 'o'
		||  theModifier == 'v')
		{
			BString myTarget (GetWord (target, targetPos++));
			NameItem *item;
			int32 pos;

			if ((pos = FindPosition (myTarget.String())) < 0
			||  (item = static_cast<NameItem *>(namesList->ItemAt (pos))) == 0)
			{
				printf("[ERROR] Couldn't find %s\n", myTarget.String());
				return;
			}

			int32 iStatus (item->Status());

			if (theOperator == '+' && theModifier == 'o')
			{
				hit = true;

				if ((iStatus & STATUS_OP_BIT) == 0)
				{
					item->SetStatus ((iStatus & ~STATUS_NORMAL_BIT) | STATUS_OP_BIT);
					++opsCount;

					buffer = "";
					buffer << opsCount;
					status->SetItemValue (STATUS_OPS, buffer.String());
				}
			}

			else if (theModifier == 'o')
			{
				hit = true;

				if ((iStatus & STATUS_OP_BIT) != 0)
				{
					iStatus &= ~STATUS_OP_BIT;
					if ((iStatus & STATUS_VOICE_BIT) == 0)
						iStatus |= STATUS_NORMAL_BIT;
					item->SetStatus (iStatus);
					--opsCount;
				
					buffer = "";
					buffer << opsCount;
					status->SetItemValue (STATUS_OPS, buffer.String());
				}
			}

			if (theOperator == '+' && theModifier == 'v')
			{
				hit = true;
				
				item->SetStatus ((iStatus & ~STATUS_NORMAL_BIT) | STATUS_VOICE_BIT);
			}
			else if (theModifier == 'v')
			{
				hit = true;

				iStatus &= ~STATUS_VOICE_BIT;
				if ((iStatus & STATUS_OP_BIT) == 0)
					iStatus |= STATUS_NORMAL_BIT;
				item->SetStatus (iStatus);
			}
		}

		else if (theModifier == 'l' && theOperator == '-')
		{
			BString myTarget (GetWord (target, targetPos++));
			UpdateMode('-', 'l');
			chanLimit = "";
		}

		else if (theModifier == 'l')
		{
			BString myTarget (GetWord (target, targetPos++));
			chanLimitOld = chanLimit;
			chanLimit = myTarget;
			UpdateMode('+', 'l');	
		}

		else if (theModifier == 'k' && theOperator == '-')
		{
			UpdateMode('-', 'k');
			chanKey = "";
		}

		else if (theModifier == 'k')
		{
			BString myTarget (GetWord (target, targetPos++));
			chanKeyOld = chanKey;
			chanKey = myTarget;
			UpdateMode('+', 'k');
		}

		else if (theModifier == 'b') 
		{
			// dont do anything else
		}

		else
		{
			UpdateMode (theOperator, theModifier);
		}

		++modPos;
		if (mode[modPos] == '+'
		||  mode[modPos] == '-')
			theOperator = mode[modPos++];
	}

	if (hit)
	{
		namesList->SortItems (SortNames);
		namesList->Invalidate();
	}
}

void
ChannelWindow::StateChange (BMessage *msg)
{
	printf ("ChannelWindow::StateChange %s\n", id.String());
	ClientWindow::StateChange (msg);

	if (msg->HasData ("color", B_RGB_COLOR_TYPE))
	{
		const rgb_color *color;
		int32 which;
		ssize_t size;

		msg->FindInt32 ("which", &which);
		msg->FindData (
			"color",
			B_RGB_COLOR_TYPE,
			reinterpret_cast<const void **>(&color),
			&size);

		namesList->SetColor (which, *color);
	}

	if (msg->HasPointer ("font"))
	{
		BFont *font;
		int32 which;

		msg->FindInt32 ("which", &which);
		msg->FindPointer ("font", reinterpret_cast<void **>(&font));

		if (which == F_NAMES)
			namesList->SetFont (which, font);

		if (which == F_INPUT)
		{
			namesScroll->ResizeTo (
				namesScroll->Frame().Width(),
				input->Frame().top - 1);
		}
	}
}
