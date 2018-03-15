/**
 * Copyright (C) 2011-2018 Aratelia Limited - Juan A. Rubio
 *
 * This file is part of Tizonia
 *
 * Tizonia is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Tizonia is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Tizonia.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   tizpandora.cpp
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  Tizonia - Simple Pandora audio client library
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <boost/lexical_cast.hpp>
#include <iostream>

#include "tizpandora.hpp"

namespace bp = boost::python;

/* This macro assumes the existence of an "int rc" local variable */
#define try_catch_wrapper(expr)                                  \
  do                                                             \
    {                                                            \
      try                                                        \
        {                                                        \
          if (!rc)                                               \
            {                                                    \
              (expr);                                            \
            }                                                    \
        }                                                        \
      catch (bp::error_already_set & e)                          \
        {                                                        \
          PyErr_PrintEx (0);                                     \
          rc = 1;                                                \
        }                                                        \
      catch (const std::exception &e)                            \
        {                                                        \
          std::cerr << e.what ();                                \
          rc = 1;                                                \
        }                                                        \
      catch (...)                                                \
        {                                                        \
          std::cerr << std::string ("Unknown exception caught"); \
          rc = 1;                                                \
        }                                                        \
    }                                                            \
  while (0)

namespace
{
  int check_deps ()
  {
    int rc = 1;
    Py_Initialize ();

    try
      {
        // Import the Tizonia Pandora proxy module
        bp::object py_main = bp::import ("__main__");

        // Retrieve the main module's namespace
        bp::object py_global = py_main.attr ("__dict__");

        // Check the existence of the 'pandoraapi' module
        bp::object ignored = exec (
            "import imp\n"
            "imp.find_module('pandoraapi')\n",
            py_global);

        // Check the existence of the 'fuzzywuzzy' module
        bp::object ignored2 = exec (
            "import imp\n"
            "imp.find_module('fuzzywuzzy')\n",
            py_global);

        rc = 0;
      }
    catch (bp::error_already_set &e)
      {
        PyErr_PrintEx (0);
        std::cerr << std::string (
            "\nPython modules 'pandoraapi' or 'fuzzywuzzy' not found."
            "\nPlease make sure these are installed correctly.\n");
      }
    catch (...)
      {
        std::cerr << std::string ("Unknown exception caught");
      }
    return rc;
  }

  void init_pandora (boost::python::object &py_main,
                  boost::python::object &py_global)
  {
    // Import the Pandora proxy module
    py_main = bp::import ("tizpandoraproxy");

    // Retrieve the main module's namespace
    py_global = py_main.attr ("__dict__");
  }

  void start_pandora (boost::python::object &py_global,
                   boost::python::object &py_pandora_proxy,
                   const std::string &base_url, const std::string &auth_token)
  {
    bp::object pypandoraproxy = py_global["tizpandoraproxy"];
    py_pandora_proxy = pypandoraproxy (base_url.c_str (), auth_token.c_str ());
  }
}

tizpandora::tizpandora (const std::string &base_url, const std::string &auth_token)
  : base_url_ (base_url),
    auth_token_ (auth_token),
    current_url_ (),
    current_track_index_ (),
    current_queue_length_ (),
    current_track_title_ (),
    current_track_artist_ (),
    current_track_album_ (),
    current_track_year_ (),
    current_track_file_size_ (),
    current_track_file_size_as_int_ (0),
    current_track_duration_ (),
    current_track_bitrate_ (),
    current_track_codec_ (),
    current_track_album_art_ (),
    current_queue_progress_ ()
{
}

tizpandora::~tizpandora ()
{
}

int tizpandora::init ()
{
  int rc = 0;
  if (0 == (rc = check_deps ()))
    {
      try_catch_wrapper (init_pandora (py_main_, py_global_));
    }
  return rc;
}

int tizpandora::start ()
{
  int rc = 0;
  try_catch_wrapper (
      start_pandora (py_global_, py_pandora_proxy_, base_url_, auth_token_));
  return rc;
}

void tizpandora::stop ()
{
  int rc = 0;
  // try_catch_wrapper (py_pandora_proxy_.attr ("logout")());
  (void)rc;
}

void tizpandora::deinit ()
{
  // boost::python doesn't support Py_Finalize() yet!
}

int tizpandora::play_audio_tracks (const std::string &tracks)
{
  int rc = 0;
  try_catch_wrapper (
      py_pandora_proxy_.attr ("enqueue_audio_tracks") (bp::object (tracks)));
  return rc;
}

int tizpandora::play_audio_artist (const std::string &artist)
{
  int rc = 0;
  try_catch_wrapper (
      py_pandora_proxy_.attr ("enqueue_audio_artist") (bp::object (artist)));
  return rc;
}

int tizpandora::play_audio_album (const std::string &album)
{
  int rc = 0;
  try_catch_wrapper (
      py_pandora_proxy_.attr ("enqueue_audio_album") (bp::object (album)));
  return rc;
}

int tizpandora::play_audio_playlist (const std::string &playlist)
{
  int rc = 0;
  try_catch_wrapper (
      py_pandora_proxy_.attr ("enqueue_audio_playlist") (bp::object (playlist)));
  return rc;
}

const char *tizpandora::get_next_url (const bool a_remove_current_url)
{
  current_url_.clear ();
  try
    {
      if (a_remove_current_url)
        {
          py_pandora_proxy_.attr ("remove_current_url") ();
        }
      const char *p_next_url
          = bp::extract< char const * > (py_pandora_proxy_.attr ("next_url") ());
      current_url_.assign (p_next_url);
      if (!p_next_url || get_current_track ())
        {
          current_url_.clear ();
        }
    }
  catch (bp::error_already_set &e)
    {
      PyErr_PrintEx (0);
    }
  catch (...)
    {
    }
  return current_url_.empty () ? NULL : current_url_.c_str ();
}

const char *tizpandora::get_prev_url (const bool a_remove_current_url)
{
  current_url_.clear ();
  try
    {
      if (a_remove_current_url)
        {
          py_pandora_proxy_.attr ("remove_current_url") ();
        }
      const char *p_prev_url
          = bp::extract< char const * > (py_pandora_proxy_.attr ("prev_url") ());
      current_url_.assign (p_prev_url);
      if (!p_prev_url || get_current_track ())
        {
          current_url_.clear ();
        }
    }
  catch (bp::error_already_set &e)
    {
      PyErr_PrintEx (0);
    }
  catch (...)
    {
    }
  return current_url_.empty () ? NULL : current_url_.c_str ();
}

void tizpandora::clear_queue ()
{
  int rc = 0;
  try_catch_wrapper (py_pandora_proxy_.attr ("clear_queue") ());
  (void)rc;
}

const char *tizpandora::get_current_audio_track_index ()
{
  return current_track_index_.empty () ? NULL : current_track_index_.c_str ();
}

const char *tizpandora::get_current_queue_length ()
{
  return current_queue_length_.empty () ? NULL : current_queue_length_.c_str ();
}

const char *tizpandora::get_current_queue_progress ()
{
  current_queue_progress_.assign (get_current_audio_track_index ());
  current_queue_progress_.append (" of ");
  current_queue_progress_.append (get_current_queue_length ());
  return current_queue_progress_.c_str ();
}

void tizpandora::set_playback_mode (const playback_mode mode)
{
  int rc = 0;
  switch (mode)
    {
      case PlaybackModeNormal:
        {
          try_catch_wrapper (py_pandora_proxy_.attr ("set_play_mode") ("NORMAL"));
        }
        break;
      case PlaybackModeShuffle:
        {
          try_catch_wrapper (py_pandora_proxy_.attr ("set_play_mode") ("SHUFFLE"));
        }
        break;
      default:
        {
          assert (0);
        }
        break;
    };
  (void)rc;
}

const char *tizpandora::get_current_audio_track_title ()
{
  return current_track_title_.empty () ? NULL : current_track_title_.c_str ();
}

const char *tizpandora::get_current_audio_track_artist ()
{
  return current_track_artist_.empty () ? NULL : current_track_artist_.c_str ();
}

const char *tizpandora::get_current_audio_track_album ()
{
  return current_track_album_.empty () ? NULL : current_track_album_.c_str ();
}

const char *tizpandora::get_current_audio_track_year ()
{
  return current_track_year_.empty () ? NULL : current_track_year_.c_str ();
}

const char *tizpandora::get_current_audio_track_file_size ()
{
  return current_track_file_size_.empty () ? NULL
                                           : current_track_file_size_.c_str ();
}

int tizpandora::get_current_audio_track_file_size_as_int ()
{
  return current_track_file_size_as_int_;
}

const char *tizpandora::get_current_audio_track_duration ()
{
  return current_track_duration_.empty () ? NULL
                                          : current_track_duration_.c_str ();
}

const char *tizpandora::get_current_audio_track_bitrate ()
{
  return current_track_bitrate_.empty () ? NULL
                                         : current_track_bitrate_.c_str ();
}

const char *tizpandora::get_current_audio_track_codec ()
{
  return current_track_codec_.empty () ? NULL : current_track_codec_.c_str ();
}

const char *tizpandora::get_current_audio_track_album_art ()
{
  return current_track_album_art_.empty () ? NULL
                                           : current_track_album_art_.c_str ();
}

int tizpandora::get_current_track ()
{
  int rc = 0;
  current_track_index_.clear ();
  current_queue_length_.clear ();
  current_track_title_.clear ();
  current_track_artist_.clear ();
  current_track_album_.clear ();
  current_track_year_.clear ();
  current_track_file_size_.clear ();
  current_track_duration_.clear ();
  current_track_bitrate_.clear ();
  current_track_codec_.clear ();
  current_track_album_art_.clear ();

  const bp::tuple &queue_info = bp::extract< bp::tuple > (py_pandora_proxy_.attr (
      "current_audio_track_queue_index_and_queue_length") ());
  const int queue_index = bp::extract< int > (queue_info[0]);
  const int queue_length = bp::extract< int > (queue_info[1]);
  current_track_index_.assign (
      boost::lexical_cast< std::string > (queue_index));
  current_queue_length_.assign (
      boost::lexical_cast< std::string > (queue_length));

  const char *p_title = bp::extract< char const * > (
      py_pandora_proxy_.attr ("current_audio_track_title") ());
  if (p_title)
    {
      current_track_title_.assign (p_title);
    }

  const char *p_artist = bp::extract< char const * > (
      py_pandora_proxy_.attr ("current_audio_track_artist") ());
  if (p_artist)
    {
      current_track_artist_.assign (p_artist);
    }

  const char *p_album = bp::extract< char const * > (
      py_pandora_proxy_.attr ("current_audio_track_album") ());
  if (p_album)
    {
      current_track_album_.assign (p_album);
    }

  const int year = bp::extract< int > (
      py_pandora_proxy_.attr ("current_audio_track_year") ());
  current_track_year_.assign (boost::lexical_cast< std::string > (year));

  const int file_size = bp::extract< int > (
      py_pandora_proxy_.attr ("current_audio_track_file_size") ());
  char file_size_str[20];
  sprintf (file_size_str, "%.2g", (float)file_size / (1024 * 1024));
  current_track_file_size_.assign (file_size_str);
  current_track_file_size_.append (" MiB");
  current_track_file_size_as_int_ = file_size;

  const int duration = bp::extract< int > (
      py_pandora_proxy_.attr ("current_audio_track_duration") ());
  int seconds = duration % 60;
  int minutes = (duration - seconds) / 60;
  int hours = 0;
  if (minutes >= 60)
    {
      int total_minutes = minutes;
      minutes = total_minutes % 60;
      hours = (total_minutes - minutes) / 60;
    }

  if (hours > 0)
    {
      current_track_duration_.assign (
          boost::lexical_cast< std::string > (hours));
      current_track_duration_.append ("h:");
    }

  if (minutes > 0)
    {
      current_track_duration_.append (
          boost::lexical_cast< std::string > (minutes));
      current_track_duration_.append ("m:");
    }

  char seconds_str[10];
  if (0 == minutes && 0 == hours)
    {
      sprintf (seconds_str, "%01i", seconds);
    }
  else
    {
      sprintf (seconds_str, "%02i", seconds);
    }
  current_track_duration_.append (seconds_str);
  current_track_duration_.append ("s");

  const int bitrate = bp::extract< int > (
      py_pandora_proxy_.attr ("current_audio_track_bitrate") ());
  current_track_bitrate_.assign (boost::lexical_cast< std::string > (bitrate));

  const char *p_codec = bp::extract< char const * > (
      py_pandora_proxy_.attr ("current_audio_track_codec") ());
  if (p_codec)
    {
      current_track_codec_.assign (p_codec);
    }

  const char *p_album_art = bp::extract< char const * > (
      py_pandora_proxy_.attr ("current_audio_track_album_art") ());
  if (p_album_art)
    {
      current_track_album_art_.assign (p_album_art);
    }

  return rc;
}
