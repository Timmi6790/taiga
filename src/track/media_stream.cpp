/*
** Taiga
** Copyright (C) 2010-2014, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "base/foreach.h"
#include "base/process.h"
#include "base/string.h"
#include "library/anime_episode.h"
#include "taiga/settings.h"
#include "track/media.h"

enum StreamingVideoProviders {
  kStreamUnknown = -1,
  kStreamAnn,
  kStreamCrunchyroll,
  kStreamHulu,
  kStreamVeoh,
  kStreamVizanime,
  kStreamYoutube
};

enum WebBrowserEngines {
  kWebEngineUnknown = -1,
  kWebEngineWebkit,   // Google Chrome (and other browsers based on Chromium)
  kWebEngineGecko,    // Mozilla Firefox
  kWebEngineTrident,  // Internet Explorer
  kWebEnginePresto    // Opera (older versions)
};

////////////////////////////////////////////////////////////////////////////////

base::AccessibleChild* FindAccessibleChild(
    std::vector<base::AccessibleChild>& children,
    const std::wstring& name,
    const std::wstring& role) {
  base::AccessibleChild* child = nullptr;

  foreach_(it, children) {
    if (name.empty() || IsEqual(name, it->name))
      if (role.empty() || IsEqual(role, it->role))
        child = &(*it);
    if (child == nullptr && !it->children.empty())
      child = FindAccessibleChild(it->children, name, role);
    if (child)
      break;
  }

  return child;
}

bool MediaPlayers::BrowserAccessibleObject::AllowChildTraverse(
    base::AccessibleChild& child,
    LPARAM param) {
  switch (param) {
    case kWebEngineUnknown:
      return false;
    case kWebEngineGecko:
      if (IsEqual(child.role, L"document"))
        return false;
      break;
    case kWebEngineTrident:
      if (IsEqual(child.role, L"pane") || IsEqual(child.role, L"scroll bar"))
        return false;
      break;
    case kWebEnginePresto:
      if (IsEqual(child.role, L"document") || IsEqual(child.role, L"pane"))
        return false;
      break;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////

std::wstring MediaPlayers::GetTitleFromBrowser(HWND hwnd) {
  int web_engine = kWebEngineUnknown;

  auto media_player = FindPlayer(current_player());

  // Get window title
  std::wstring title = GetWindowTitle(hwnd);
  EditTitle(title, media_player);

  // Return current title if the same web page is still open
  if (CurrentEpisode.anime_id > 0)
    if (InStr(title, current_title()) > -1)
      return current_title();

  // Delay operation to save some CPU
  static int counter = 0;
  if (counter < 5) {
    counter++;
    return current_title();
  } else {
    counter = 0;
  }

  // Select web browser engine
  if (media_player->engine == L"WebKit") {
    web_engine = kWebEngineWebkit;
  } else if (media_player->engine == L"Gecko") {
    web_engine = kWebEngineGecko;
  } else if (media_player->engine == L"Trident") {
    web_engine = kWebEngineTrident;
  } else if (media_player->engine == L"Presto") {
    web_engine = kWebEnginePresto;
  } else {
    return std::wstring();
  }

  // Build accessibility data
  acc_obj.children.clear();
  if (acc_obj.FromWindow(hwnd) == S_OK) {
    acc_obj.BuildChildren(acc_obj.children, nullptr, web_engine);
    acc_obj.Release();
  }

  // Check other tabs
  if (CurrentEpisode.anime_id > 0) {
    base::AccessibleChild* child = nullptr;
    switch (web_engine) {
      case kWebEngineWebkit:
      case kWebEngineGecko:
        child = FindAccessibleChild(acc_obj.children, L"", L"page tab list");
        break;
      case kWebEngineTrident:
        child = FindAccessibleChild(acc_obj.children, L"Tab Row", L"");
        break;
      case kWebEnginePresto:
        child = FindAccessibleChild(acc_obj.children, L"", L"client");
        break;
    }
    if (child) {
      foreach_(it, child->children) {
        if (InStr(it->name, current_title()) > -1) {
          // Tab is still open, just not active
          return current_title();
        }
      }
    }
    // Tab is closed
    return std::wstring();
  }

  // Find URL
  base::AccessibleChild* child = nullptr;
  switch (web_engine) {
    case kWebEngineWebkit:
      child = FindAccessibleChild(acc_obj.children,
                                  L"Address and search bar",
                                  L"grouping");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Address",
                                    L"grouping");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Location",
                                    L"grouping");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Address field",
                                    L"editable text");
      break;
    case kWebEngineGecko:
      child = FindAccessibleChild(acc_obj.children,
                                  L"Search or enter address",
                                  L"editable text");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Go to a Website",
                                    L"editable text");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Go to a Web Site",
                                    L"editable text");
      break;
    case kWebEngineTrident:
      child = FindAccessibleChild(acc_obj.children,
                                  L"Address and search using Bing",
                                  L"editable text");
      if (child == nullptr)
        child = FindAccessibleChild(acc_obj.children,
                                    L"Address and search using Google",
                                    L"editable text");
      break;
    case kWebEnginePresto:
      child = FindAccessibleChild(acc_obj.children,
                                  L"", L"client");
      if (child && !child->children.empty()) {
        child = FindAccessibleChild(child->children.at(0).children,
                                    L"", L"tool bar");
        if (child && !child->children.empty()) {
          child = FindAccessibleChild(child->children,
                                      L"", L"combo box");
          if (child && !child->children.empty()) {
            child = FindAccessibleChild(child->children,
                                        L"", L"editable text");
          }
        }
      }
      break;
  }

  if (child) {
    title = GetTitleFromStreamingMediaProvider(child->value, title);
  } else {
    title.clear();
  }

  return title;
}

std::wstring MediaPlayers::GetTitleFromStreamingMediaProvider(
    const std::wstring& url,
    std::wstring& title) {
  int stream_provider = kStreamUnknown;

  // Check URL for known streaming video providers
  if (!url.empty()) {
    // Anime News Network
    if (Settings.GetBool(taiga::kStream_Ann) &&
        InStr(url, L"animenewsnetwork.com/video") > -1) {
      stream_provider = kStreamAnn;
    // Crunchyroll
    } else if (Settings.GetBool(taiga::kStream_Crunchyroll) &&
               InStr(url, L"crunchyroll.com/") > -1) {
       stream_provider = kStreamCrunchyroll;
    // Hulu
    /*
    } else if (InStr(url, L"hulu.com/watch") > -1) {
      stream_provider = kStreamHulu;
    */
    // Veoh
    } else if (Settings.GetBool(taiga::kStream_Veoh) &&
               InStr(url, L"veoh.com/watch") > -1) {
      stream_provider = kStreamVeoh;
    // Viz Anime
    } else if (Settings.GetBool(taiga::kStream_Viz) &&
               InStr(url, L"vizanime.com/ep") > -1) {
      stream_provider = kStreamVizanime;
    // YouTube
    } else if (Settings.GetBool(taiga::kStream_Youtube) &&
               InStr(url, L"youtube.com/watch") > -1) {
      stream_provider = kStreamYoutube;
    }
  }

  // Clean-up title
  switch (stream_provider) {
    // Anime News Network
    case kStreamAnn:
      EraseRight(title, L" - Anime News Network");
      Erase(title, L" (s)");
      Erase(title, L" (d)");
      break;
    // Crunchyroll
    case kStreamCrunchyroll:
      EraseLeft(title, L"Crunchyroll - Watch ");
      break;
    // Hulu
    case kStreamHulu:
      EraseLeft(title, L"Watch ");
      EraseRight(title, L" online | Free | Hulu");
      EraseRight(title, L" online | Plus | Hulu");
      break;
    // Veoh
    case kStreamVeoh:
      EraseLeft(title, L"Watch Videos Online | ");
      EraseRight(title, L" | Veoh.com");
      break;
    // Viz Anime
    case kStreamVizanime:
      EraseRight(title, L" - VIZ ANIME: Free Online Anime - All The Time");
      break;
    // YouTube
    case kStreamYoutube:
      EraseRight(title, L" - YouTube");
      break;
    // Some other website, or URL is not found
    default:
    case kStreamUnknown:
      title.clear();
      break;
  }

  return title;
}