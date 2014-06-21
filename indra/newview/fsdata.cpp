/* Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "fsdata.h"

#include "llbufferstream.h"

#include "llappviewer.h"

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>

#include "llsdserialize.h"

#include "llversionviewer.h"
#include "llprimitive.h"
#include "llagent.h"
#include "llnotifications.h"
#include "llimview.h"
#include "llfloaterabout.h"
#include "llviewercontrol.h"
//#include "floaterblacklist.h"
#include "llsys.h"
#include "llviewermedia.h"
#include "llagentui.h"

static const std::string versionid = llformat("%s %d.%d.%d (%d)", LL_CHANNEL, LL_VERSION_MAJOR, LL_VERSION_MINOR, LL_VERSION_PATCH, LL_VERSION_BUILD);
static const std::string fsdata_url = "http://phoenixviewer.com/app/fsdata/data.xml";
static const std::string releases_url = "http://phoenixviewer.com/app/fsdata/releases.xml";
static const std::string agents_url = "http://phoenixviewer.com/app/fsdata/agents.xml";
//static const std::string blacklist_url = "http://phoenixviewer.com/app/fsdata/blacklist.xml";

class FSDownloader : public LLHTTPClient::Responder
{
public:
	FSDownloader(std::string filename) :
		mFilename(filename)
	{
	}
	
	virtual void completedRaw(U32 status,
				const std::string& reason,
				const LLChannelDescriptors& channels,
				const LLIOPipe::buffer_ptr_t& buffer)
	{
		LLBufferStream istr(channels, buffer.get());
		std::stringstream strstrm;
		strstrm << istr.rdbuf();
		std::string result = std::string(strstrm.str());
		// hack: use the filename or url passed to determine what function to pass the data onto.
		// TODO; merge the seperate functions into one.
		if (mFilename == fsdata_url)
		{
			FSData::getInstance()->processData(status, result);
		}
		if (mFilename == releases_url)
		{
			FSData::getInstance()->processReleases(status, result);
		}
		if (mFilename == agents_url)
		{
			FSData::getInstance()->processAgents(status, result);
		}
	}
	
private:
	std::string mFilename;
};



std::string FSData::blacklist_version;
LLSD FSData::blocked_login_info = 0;
LLSD FSData::phoenix_tags = 0;
BOOL FSData::msDataDone = FALSE;

FSData* FSData::sInstance;

FSData::FSData()
{
	sInstance = this;
}

FSData::~FSData()
{
	sInstance = NULL;
}

FSData* FSData::getInstance()
{
	if(sInstance)return sInstance;
	else
	{
		sInstance = new FSData();
		return sInstance;
	}
}



void FSData::startDownload()
{
	LLSD headers;
	headers.insert("User-Agent", LLViewerMedia::getCurrentUserAgent());
	headers.insert("viewer-version", versionid);
	LL_INFOS("Data") << "Downloading data.xml" << LL_ENDL;
	LLHTTPClient::get(fsdata_url,new FSDownloader(fsdata_url),headers);
}
	
void FSData::processData(U32 status, std::string body)
{
	if(status != 200)
	{
		LL_WARNS("Data") << "data.xml download failed with status of " << status << LL_ENDL;
		return;
	}
	LLSD fsData;
	std::istringstream istr(body);
	LLSDSerialize::fromXML(fsData, istr);
	if(!fsData.isDefined())
	{
	  LL_WARNS("Data") << "Bad data in data.xml, aborting" << LL_ENDL;
	  return;
	}

	// Set Message Of The Day if present
	if(fsData.has("MOTD"))
	{
		gAgent.mMOTD.assign(fsData["MOTD"]);
	}

	FSData* self = getInstance();
	bool local_file = false;
	if (!(fsData["Releases"].asInteger() == 0))
	{
		const std::string releases_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "releases.xml");
		llifstream releases_file(releases_filename);
		LLSD releases;
		if(releases_file.is_open())
		{
			if(LLSDSerialize::fromXML(releases, releases_file) >= 1)
			{
				if(releases.has("ReleaseVersion"))
				{
					if (fsData["Releases"].asInteger() <= releases["ReleaseVersion"].asInteger())
					{
						LLSD& fs_versions = releases["FirestormReleases"];
						self->versions2.clear();
						for(LLSD::map_iterator itr = fs_versions.beginMap(); itr != fs_versions.endMap(); ++itr)
						{
							std::string key = (*itr).first;
							key += "\n";
							LLSD& content = (*itr).second;
							U8 val = 0;
							if(content.has("beta"))val = val | PH_BETA;
							if(content.has("release"))val = val | PH_RELEASE;
							self->versions2[key] = val;
						}
						
						if(releases.has("BlockedReleases"))
						{
							LLSD& blocked = releases["BlockedReleases"];
							self->blocked_versions.clear();
							for (LLSD::map_iterator itr = blocked.beginMap(); itr != blocked.endMap(); ++itr)
							{
								std::string vers = itr->first;
								LLSD& content = itr->second;
								//LLSDcontent tmpContent;
								//tmpContent.content = content;
								self->blocked_versions[vers] = content;
							}
						}
						local_file = true;
					}
				}
			}
			releases_file.close();
		}
	}

	if (!local_file)
	{
		LL_INFOS("Data") << "Downloading " << releases_url << LL_ENDL;
		LLHTTPClient::get(releases_url,new FSDownloader(releases_url));
	}


	local_file = false;
	if (!(fsData["Agents"].asInteger() == 0))
	{
		const std::string agents_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "agents.xml");
		llifstream agents_file(agents_filename);
		LLSD agents;
		if(agents_file.is_open())
		{
			if(LLSDSerialize::fromXML(agents, agents_file) >= 1)
			{
				if(agents.has("AgentsVersion"))
				{
					if (fsData["Agents"].asInteger() <= agents["AgentsVersion"].asInteger())
					{
						LLSD& support_agents = agents["SupportAgents"];
						self->personnel.clear();
						for(LLSD::map_iterator itr = support_agents.beginMap(); itr != support_agents.endMap(); ++itr)
						{
							std::string key = (*itr).first;
							LLSD& content = (*itr).second;
							U8 val = 0;
							if(content.has("support"))val = val | EM_SUPPORT;
							if(content.has("developer"))val = val | EM_DEVELOPER;
							self->personnel[LLUUID(key)] = val;
						}
						local_file = true;
					}
				}
			}
			agents_file.close();
		}
	}

	if (!local_file)
	{
		LL_INFOS("Data") << "Downloading " << agents_url << LL_ENDL;
		LLHTTPClient::get(agents_url,new FSDownloader(agents_url));
	}

	//TODO: add blacklist support
// 	LL_INFOS("Blacklist") << "Downloading blacklist.xml" << LL_ENDL;
// 	LLHTTPClient::get(url,new FSDownloader( FSData::msblacklist ),headers);

	//TODO: add legisity client tags
	//downloadClientTags();
}

void FSData::processReleases(U32 status, std::string body)
{
  	if(status != 200)
	{
		LL_WARNS("Releases") << "releases.xml download failed with status of " << status << LL_ENDL;
		return;
	}
  
	FSData* self = getInstance();
	LLSD releases;
	std::istringstream istr(body);
	LLSDSerialize::fromXML(releases, istr);
	if(releases.isDefined())
	{
		LLSD& fs_versions = releases["FirestormReleases"];
		self->versions2.clear();
		for(LLSD::map_iterator itr = fs_versions.beginMap(); itr != fs_versions.endMap(); ++itr)
		{
			std::string key = (*itr).first;
			key += "\n";
			LLSD& content = (*itr).second;
			U8 val = 0;
			if(content.has("beta"))val = val | PH_BETA;
			if(content.has("release"))val = val | PH_RELEASE;
			self->versions2[key] = val;
		}
		
		if(releases.has("BlockedReleases"))
		{
			LLSD& blocked = releases["BlockedReleases"];
			self->blocked_versions.clear();
			for (LLSD::map_iterator itr = blocked.beginMap(); itr != blocked.endMap(); ++itr)
			{
				std::string vers = itr->first;
				LLSD& content = itr->second;
				self->blocked_versions[vers] = content;
			}
		}
		
		// save the download to a file
		const std::string releases_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "releases.xml");
		LL_INFOS("Data") << "Saving " << releases_filename << LL_ENDL;
		llofstream releases_file;
		releases_file.open(releases_filename);
		LLSDSerialize::toPrettyXML(releases, releases_file);
		releases_file.close();
	}
}

void FSData::processAgents(U32 status, std::string body)
{
  	if(status != 200)
	{
		LL_WARNS("Agents") << "agents.xml download failed with status of " << status << LL_ENDL;
		return;
	}
  
	FSData* self = getInstance();
	LLSD agents;
	std::istringstream istr(body);
	LLSDSerialize::fromXML(agents, istr);
	if(agents.isDefined())
	{
		LLSD& support_agents = agents["SupportAgents"];
		self->personnel.clear();
		for(LLSD::map_iterator itr = support_agents.beginMap(); itr != support_agents.endMap(); ++itr)
		{
			std::string key = (*itr).first;
			LLSD& content = (*itr).second;
			U8 val = 0;
			if(content.has("support"))val = val | EM_SUPPORT;
			if(content.has("developer"))val = val | EM_DEVELOPER;
			self->personnel[LLUUID(key)] = val;
		}
		
		// save the download to a file
		const std::string agents_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "agents.xml");
		LL_INFOS("Data") << "Saving " << agents_filename << LL_ENDL;
		llofstream agents_file;
		agents_file.open(agents_filename);
		LLSDSerialize::toPrettyXML(agents, agents_file);
		agents_file.close();
	}
}

//TODO: add legisity tags support
#if (0)
void FSData::downloadClientTags()
{
	if(gSavedSettings.getBOOL("PhoenixDownloadClientTags"))
	{
		//url = "http://phoenixviewer.com/app/client_tags/client_list.xml";
		std::string url("http://phoenixviewer.com/app/client_tags/client_list_v2.xml");
		// if(gSavedSettings.getBOOL("PhoenixDontUseMultipleColorTags"))
		// {
		//	 url="http://phoenixviewer.com/app/client_list_unified_colours.xml";
		// }
		LLSD headers;
		LLHTTPClient::get(url,new FSDownloader( FSData::updateClientTags),headers);
		LL_INFOS("CLIENTTAGS DOWNLOADER") << "Getting new tags" << LL_ENDL;
	}
	else
	{
		updateClientTagsLocal();
	}
	
}

void FSData::msblacklist(U32 status,std::string body)
{
	if(status != 200)
	{
		LL_WARNS("Blacklist") << "Something went wrong with the blacklist download status code " << status << LL_ENDL;
	}

	std::istringstream istr(body);
	if (body.size() > 0)
	{
		LLSD data;
		if(LLSDSerialize::fromXML(data,istr) >= 1)
		{
			LL_INFOS("Blacklist") << body.size() << " bytes received, updating local blacklist" << LL_ENDL;
			for(LLSD::map_iterator itr = data.beginMap(); itr != data.endMap(); ++itr)
			{
				if(itr->second.has("name"))
					LLFloaterBlacklist::addEntry(LLUUID(itr->first),itr->second);
			}
		}
		else
		{
			LL_INFOS("Blacklist") << "Failed to parse blacklist.xml" << LL_ENDL;
		}
	}
	else
	{
		LL_INFOS("Blacklist") << "Empty blacklist.xml" << LL_ENDL;
	}
}

void FSData::updateClientTags(U32 status,std::string body)
{
    std::string client_list_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "client_list_v2.xml");

    std::istringstream istr(body);
    LLSD data;
    if(LLSDSerialize::fromXML(data, istr) >= 1)
	{
		llofstream export_file;
        export_file.open(client_list_filename);
        LLSDSerialize::toPrettyXML(data, export_file);
        export_file.close();
    }
}

void FSData::updateClientTagsLocal()
{
	std::string client_list_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "client_list_v2.xml");

	llifstream xml_file(client_list_filename);
	LLSD data;
	if(!xml_file.is_open()) return;
	if(LLSDSerialize::fromXML(data, xml_file) >= 1)
	{
		if(data.has("phoenixTags"))
		{
			phoenix_tags = data["phoenixTags"];
			LLPrimitive::tagstring = FSData::phoenix_tags[gSavedSettings.getString("PhoenixTagColor")].asString();
		}

		xml_file.close();
	}
}

void FSData::msdata(U32 status, std::string body)
{
	FSData* self = getInstance();
	//cmdline_printchat("msdata downloaded");

	LLSD data;
	std::istringstream istr(body);
	LLSDSerialize::fromXML(data, istr);
	if(data.isDefined())
	{

// Removed code chunks as they are ported to help keep track of what needs done. -- Techwolf Lupindo


		if(data.has("phoenixTags"))
		{
			phoenix_tags = data["phoenixTags"];
			LLPrimitive::tagstring = FSData::phoenix_tags[gSavedSettings.getString("PhoenixTagColor")].asString();
		}
		msDataDone = TRUE;
	}

	//LLSD& dev_agents = data["dev_agents"];
	//LLSD& client_ids = data["client_ids"];
}
#endif

BOOL FSData::is_support(LLUUID id)
{
	FSData* self = getInstance();
	if(self->personnel.find(id) != self->personnel.end())
	{
		return ((self->personnel[id] & EM_SUPPORT) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

BOOL FSData::is_BetaVersion(std::string version)
{
	FSData* self = getInstance();
	if(self->versions2.find(version) != self->versions2.end())
	{
		return ((self->versions2[version] & PH_BETA) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

BOOL FSData::is_ReleaseVersion(std::string version)
{
	FSData* self = getInstance();
	if(self->versions2.find(version) != self->versions2.end())
	{
		return ((self->versions2[version] & PH_RELEASE) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

BOOL FSData::is_developer(LLUUID id)
{
	FSData* self = getInstance();
	if(self->personnel.find(id) != self->personnel.end())
	{
		return ((self->personnel[id] & EM_DEVELOPER) != 0) ? TRUE : FALSE;
	}
	return FALSE;
}

LLSD FSData::allowed_login()
{
	FSData* self = getInstance();
	std::map<std::string, LLSD>::iterator iter = self->blocked_versions.find(versionid);
	if (iter == self->blocked_versions.end())
	{
		LLSD empty;
		return empty; 
	}
	else
	{
		return iter->second;
	}
}


std::string FSData::processRequestForInfo(LLUUID requester, std::string message, std::string name, LLUUID sessionid)
{
	std::string detectstring = "/reqsysinfo";
	if(!message.find(detectstring)==0)
	{
		//llinfos << "sysinfo was not found in this message, it was at " << message.find("/sysinfo") << " pos." << llendl;
		return message;
	}
	if(!(is_support(requester)||is_developer(requester)))
	{
		return message;
	}

	//llinfos << "sysinfo was found in this message, it was at " << message.find("/sysinfo") << " pos." << llendl;
	std::string outmessage("I am requesting information about your system setup.");
	std::string reason("");
	if(message.length()>detectstring.length())
	{
		reason = std::string(message.substr(detectstring.length()));
		//there is more to it!
		outmessage = std::string("I am requesting information about your system setup for this reason : "+reason);
		reason = "The reason provided was : "+reason;
	}
	LLSD args;
	args["REASON"] =reason;
	args["NAME"] = name;
	args["FROMUUID"]=requester;
	args["SESSIONID"]=sessionid;
	LLNotifications::instance().add("FireStormReqInfo",args,LLSD(), callbackReqInfo);

	return outmessage;
}
void FSData::sendInfo(LLUUID destination, LLUUID sessionid, std::string myName, EInstantMessage dialog)
{

	std::string myInfo1 = getMyInfo(1);
//	std::string myInfo2 = getMyInfo(2);	

	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		destination,
		myName,
		myInfo1,
		IM_ONLINE,
		dialog,
		sessionid
		);
	gAgent.sendReliableMessage();
// 	pack_instant_message(
// 		gMessageSystem,
// 		gAgent.getID(),
// 		FALSE,
// 		gAgent.getSessionID(),
// 		destination,
// 		myName,
// 		myInfo2,
// 		IM_ONLINE,
// 		dialog,
// 		sessionid);
// 	gAgent.sendReliableMessage();
	gIMMgr->addMessage(gIMMgr->computeSessionID(dialog,destination),destination,myName,"Information Sent: "+
		myInfo1); //+"\n"+myInfo2);
}
void FSData::callbackReqInfo(const LLSD &notification, const LLSD &response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string my_name;
	LLSD subs = LLNotification(notification).getSubstitutions();
	LLUUID uid = subs["FROMUUID"].asUUID();
	LLUUID sessionid = subs["SESSIONID"].asUUID();

	llinfos << "the uuid is " << uid.asString().c_str() << llendl;
	LLAgentUI::buildFullname(my_name);
	//LLUUID sessionid = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL,uid);
	if ( option == 0 )//yes
	{
		sendInfo(uid,sessionid,my_name,IM_NOTHING_SPECIAL);
	}
	else
	{

		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			uid,
			my_name,
			"Request Denied.",
			IM_ONLINE,
			IM_NOTHING_SPECIAL,
			sessionid
			);
		gAgent.sendReliableMessage();
		gIMMgr->addMessage(sessionid,uid,my_name,"Request Denied");
	}
}
//part , 0 for all, 1 for 1st half, 2 for 2nd
std::string FSData::getMyInfo(int part)
{
	//copied from Zi llimfloater sendinfobutton function.
	//TODO: create a single function to elemenate code dupe.
	LLSD info=LLFloaterAbout::getInfo();

	std::ostringstream support;
	support <<
		info["CHANNEL"] << " " << info["VIEWER_VERSION_STR"] << "\n" <<
		"Sim: " << info["HOSTNAME"] << "(" << info["HOSTIP"] << ") " << info["SERVER_VERSION"] << "\n" <<
		"Packet loss: " << info["PACKETS_PCT"].asReal() << "% (" << info["PACKETS_IN"].asReal() << "/" << info["PACKETS_LOST"].asReal() << ")\n" <<
		"CPU: " << info["CPU"] << "\n" <<
		"Memory: " << info["MEMORY_MB"] << "\n" <<
		"OS: " << info["OS_VERSION"] << "\n" <<
		"GPU: " << info["GRAPHICS_CARD_VENDOR"] << " " << info["GRAPHICS_CARD"] << "\n";

	if(info.has("GRAPHICS_DRIVER_VERSION"))
		support << "Driver: " << info["GRAPHICS_DRIVER_VERSION"] << "\n";

	support <<
		"OpenGL: " << info["OPENGL_VERSION"] << "\n" <<
		"Skin: " << info["SKIN"] << "(" << info["THEME"] << ")\n" <<
		"RLV: " << info["RLV_VERSION"] << "\n" <<
		"Curl: " << info ["LIBCURL_VERSION"] << "\n" <<
		"J2C: " << info["J2C_VERSION"] << "\n" <<
		"Audio: " << info["AUDIO_DRIVER_VERSION"] << "\n" <<
		"Webkit: " << info["QT_WEBKIT_VERSION"] << "\n" <<
		"Voice: " << info["VOICE_VERSION"] << "\n" <<
		"Compiler: " << info["COMPILER"] << " Version " << info["COMPILER_VERSION"].asInteger() << "\n"
		;
	
	return support.str();
}

