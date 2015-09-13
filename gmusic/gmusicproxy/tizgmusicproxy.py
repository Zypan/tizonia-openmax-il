# Copyright (C) 2015 Aratelia Limited - Juan A. Rubio
#
# Portions Copyright (C) 2014 Dan Nixon
# (see https://github.com/DanNixon/PlayMusicCL)
#
# This file is part of Tizonia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Simple Google PLay Music proxy class.

Access a user's Google Music account to retrieve song URLs to be used for
streaming. With ideas from Dan Nixon's command-line client, which in turn
uses Simon Weber's 'Unofficial Google Music API' Python module. For more
information:

- https://github.com/DanNixon/PlayMusicCL
- https://github.com/simon-weber/Unofficial-Google-Music-API

"""

import thread
import random
import sys
import os
import logging
import unicodedata as ud
from gmusicapi import Mobileclient
from gmusicapi.exceptions import CallFailure, NotLoggedIn
from requests.structures import CaseInsensitiveDict
import pprint

logging.captureWarnings(True)
logging.getLogger().addHandler(logging.NullHandler())
logging.getLogger().setLevel(logging.INFO)

def exceptionHandler(exception_type, exception, traceback):
    print "%s" % (exception)

sys.excepthook = exceptionHandler

class tizgmusicproxy(object):
    """A class for logging into a Google Play Music account and retrieving song
    URLs.

    """

    all_songs_album_title = "All Songs"
    thumbs_up_playlist_name = "Thumbs Up"

    def __init__(self, email, password, device_id):
        self.__api = Mobileclient()
        self.logged_in = False
        self.__device_id = device_id
        self.queue = list()
        self.queue_index = -1
        self.play_mode = 0
        self.now_playing_song = None

        attempts = 0
        while not self.logged_in and attempts < 3:
            self.logged_in = self.__api.login(email, password, device_id)
            attempts += 1

        self.playlists = CaseInsensitiveDict()
        self.stations = CaseInsensitiveDict()
        self.library = CaseInsensitiveDict()

    def logout(self):
        self.__api.logout()

    def update_local_lib(self):
        songs = self.__api.get_all_songs()
        self.playlists[self.thumbs_up_playlist_name] = list()

        # Retrieve the user's song library
        song_map = dict()
        for song in songs:
            if "rating" in song and song['rating'] == "5":
                self.playlists[self.thumbs_up_playlist_name].append(song)

            song_id = song['id']
            song_artist = song['artist']
            song_album = song['album']

            song_map[song_id] = song

            if song_artist == "":
                song_artist = "Unknown Artist"

            if song_album == "":
                song_album = "Unknown Album"

            if not (song_artist in self.library):
                self.library[song_artist] = dict()
                self.library[song_artist][self.all_songs_album_title] = list()

            if not (song_album in self.library[song_artist]):
                self.library[song_artist][song_album] = list()

            self.library[song_artist][song_album].append(song)
            self.library[song_artist][self.all_songs_album_title].append(song)

        # Sort albums by track number
        for artist in self.library.keys():
            logging.info ("Artist : {0}".format(artist.encode("utf-8")))
            for album in self.library[artist].keys():
                logging.info ("   Album : {0}".format(album.encode("utf-8")))
                if album == self.all_songs_album_title:
                    sorted_album = sorted(self.library[artist][album],
                                          key=lambda k: k['title'])
                else:
                    sorted_album = sorted(self.library[artist][album],
                                          key=lambda k: k.get('trackNumber',
                                                              0))
                self.library[artist][album] = sorted_album

        # Get all user playlists
        plists = self.__api.get_all_user_playlist_contents()
        for plist in plists:
            plist_name = plist['name']
            logging.info ("playlist name : {0}".format(plist_name.encode("utf-8")))
            self.playlists[plist_name] = list()
            for track in plist['tracks']:
                try:
                    song = song_map[track['trackId']]
                    self.playlists[plist_name].append(song)
                except IndexError:
                    pass

        # Get shared playlists (All Access)
        plists_subscribed_to = [p for p in self.__api.get_all_playlists() if p.get('type') == 'SHARED']
        for plist in plists_subscribed_to:
            share_tok = plist['shareToken']
            playlist_items = self.__api.get_shared_playlist_contents(share_tok)
            plist_name = plist['name']
            logging.info ("shared playlist name : {0}".format(plist_name.encode("utf-8")))
            self.playlists[plist_name] = list()
            for item in playlist_items:
                try:
                    song = item['track']
                    song['id'] = item['trackId']
                    self.playlists[plist_name].append(song)
                except IndexError:
                    pass

        # Get stations (All Access)
        stations = self.__api.get_all_stations()
        self.stations["I'm Feeling Lucky"] = 'IFL'
        for station in stations:
            station_name = station['name']
            logging.info ("station name : {0}".format(station_name.encode("utf-8")))
            self.stations[station_name] = station['id']

    def current_song_title_and_artist(self):
        logging.info ("current_song_title_and_artist")
        song = self.now_playing_song
        if song is not None:
            title = self.now_playing_song['title']
            artist = self.now_playing_song['artist']
            logging.info ("Now playing {0} by {1}".format(title.encode("utf-8"),
                                                   artist.encode("utf-8")))
            return artist.encode("utf-8"), title.encode("utf-8")
        else:
            return '', ''

    def current_song_album_and_duration(self):
        logging.info ("current_song_album_and_duration")
        song = self.now_playing_song
        if song is not None:
            album = self.now_playing_song['album']
            duration = self.now_playing_song['durationMillis']
            logging.info ("album {0} duration {1}".format(album.encode("utf-8"),
                                                   duration.encode("utf-8")))
            return album.encode("utf-8"), int(duration)
        else:
            return '', 0

    def current_song_track_number_and_total_tracks(self):
        logging.info ("current_song_track_number_and_total_tracks")
        song = self.now_playing_song
        if song is not None:
            track = 0
            total = 0
            try:
                track = self.now_playing_song['trackNumber']
                total = self.now_playing_song['totalTrackCount']
                logging.info ("track number {0} total tracks {1}".format(track, total))
            except KeyError:
                logging.info ("trackNumber or totalTrackCount : not found")
            return track, total
        else:
            logging.info ("current_song_track_number_and_total_tracks : not found")
            return 0, 0

    def clear_queue(self):
        self.queue = list()
        self.queue_index = -1

    def enqueue_artist(self, arg):
        try:
            if not arg in self.library.keys():
                artist = next((v for (k,v) in self.library.items() if arg in k), None)
                if artist:
                    for name, art in self.library.items():
                        if art == artist:
                            print "'{0}' not found. Playing '{1}' instead.".format(arg, name)
                            break
                else:
                    raise KeyError("Artist not found : {0}".format(arg))
            else:
                artist = self.library[arg]
            count = 0
            for album in artist:
                for song in artist[album]:
                    self.queue.append(song)
                    count += 1
            logging.info ("Added {0} tracks by {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Artist not found : {0}".format(arg))

    def enqueue_album(self, arg):
        try:
            for artist in self.library:
                for album in self.library[artist]:
                    logging.info ("enqueue album : {0} | {1}".format(
                        artist.encode("utf-8"),
                        album.encode("utf-8")))
                    if album.lower() == arg.lower():
                        count = 0
                        for song in self.library[artist][album]:
                            self.queue.append(song)
                            count += 1
                        logging.info ("Added {0} tracks from {1} by "
                        "{2} to queue".format(count, album.encode("utf-8"),
                                              artist.encode("utf-8")))
        except KeyError:
            raise KeyError("Album not found : {0}".format(arg))

    def enqueue_playlist(self, arg):
        try:
            if not arg in self.playlists.keys():
                playlist = next((v for (k,v) in self.playlists.items() if arg in k), None)
                if playlist:
                    for name, plist in self.playlists.items():
                        if plist == playlist:
                            print "'{0}' not found. Playing '{1}' instead.".format(arg, name)
                            break
                else:
                    raise KeyError("Playlist not found : {0}".format(arg))
            else:
                playlist = self.playlists[arg]
            count = 0
            for song in playlist:
                self.queue.append(song)
                count += 1
            logging.info ("Added {0} tracks from {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Playlist not found : {0}".format(arg))

    def enqueue_station(self, arg):
        try:
            if not arg in self.stations.keys():
                station_id = next((v for (k,v) in self.stations.iteritems() if arg in k), None)
                if station_id:
                    for name, st_id in self.stations.iteritems():
                        if st_id == station_id:
                            print "'{0}' not found. Playing '{1}' instead.".format(arg, name)
                            break
                else:
                    raise KeyError("Station not found : {0}".format(arg))
            else:
                station_id = self.stations[arg]
            num_tracks = 200
            tracks = self.__api.get_station_tracks(station_id, num_tracks)
            count = 0
            for track in tracks:
                track[u'id'] = track['nid']
                self.queue.append(track)
                count += 1
            logging.info ("Added {0} tracks from {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Station not found : {0}".format(arg))

    def enqueue_artist_all_access(self, arg):
        try:
            artist_hits = self.__api.search_all_access(arg)['artist_hits']
            artist = next((hit for hit in artist_hits if 'best_result' in hit.keys()), None)
            if not artist:
                artist = artist_hits[0]
                print "'{0}' not found. Playing '{1}' instead.".format(arg, artist['artist']['name'])
            include_albums = False
            max_top_tracks = 50
            max_rel_artist = 0
            artist_tracks = self.__api.get_artist_info(artist['artist']['artistId'],
                                                       include_albums, max_top_tracks,
                                                       max_rel_artist)['topTracks']
            count = 0
            for track in artist_tracks:
                if not u'id' in track.keys():
                    track[u'id'] = track['nid']
                self.queue.append(track)
                count += 1
            logging.info ("Added {0} tracks from {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Artist not found : {0}".format(arg))

    def enqueue_album_all_access(self, arg):
        try:
            album_hits = self.__api.search_all_access(arg)['album_hits']
            album = next((hit for hit in album_hits if 'best_result' in hit.keys()), None)
            if not album:
                album = album_hits[0]
                print "'{0}' not found. Playing '{1}' instead.".format(arg, album['album']['name'])
            album_tracks = self.__api.get_album_info(album['album']['albumId'])['tracks']
            count = 0
            for track in album_tracks:
                if not u'id' in track.keys():
                    track[u'id'] = track['nid']
                self.queue.append(track)
                count += 1
            logging.info ("Added {0} tracks from {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Album not found : {0}".format(arg))

    def enqueue_playlist_all_access(self, arg):
        try:
            playlist_hits = self.__api.search_all_access(arg)['playlist_hits']
            playlist = next((hit for hit in playlist_hits if 'best_result' in hit.keys()), None)
            if not playlist:
                playlist = playlist_hits[0]
                print "'{0}' not found. Playing '{1}' instead.".format(arg, playlist['playlist']['name'])
            share_tok = playlist['playlist']['shareToken']
            playlist_items = self.__api.get_shared_playlist_contents(share_tok)
            count = 0
            for item in playlist_items:
                track = item['track']
                track['id'] = item['trackId']
                self.queue.append(track)
                count += 1
            logging.info ("Added {0} tracks from {1} to queue".format(count, arg))
        except KeyError:
            raise KeyError("Playlist not found : {0}".format(arg))

    def enqueue_promoted_tracks_all_access(self):
        try:
            tracks = self.__api.get_promoted_songs()
            count  = 0
            for track in tracks:
                store_track = self.__api.get_track_info(track['storeId'])
                if not u'id' in store_track.keys():
                    store_track[u'id'] = store_track['nid']
                self.queue.append(store_track)
                count += 1
            logging.info ("Added {0} All Access promoted tracks to queue".format(count))
        except CallFailure:
            raise CallFailure("Operation requires an All Access subscription.")

    def next_url(self):
        logging.info ("next_url")
        if len(self.queue):
            self.queue_index += 1
            if (self.queue_index < len(self.queue)) \
               and (self.queue_index >= 0):
                next_song = self.queue[self.queue_index]
                return self.__get_song_url(next_song)
            else:
                self.queue_index = -1
                return self.next_url()
        else:
            return ''

    def prev_url(self):
        if len(self.queue):
            self.queue_index -= 1
            if (self.queue_index < len(self.queue)) \
               and (self.queue_index >= 0):
                prev_song = self.queue[self.queue_index]
                return self.__get_song_url(prev_song)
            else:
                self.queue_index = len(self.queue)
                return self.prev_url()
        else:
            return ''

    def __get_song_url(self, song):
        logging.info ("__get_song_url : {0}".format(song['id']))
        song_url = self.__api.get_stream_url(song['id'], self.__device_id)
        try:
            self.now_playing_song = song
            return song_url
        except AttributeError:
            logging.info ("Could not retrieve song url!")
            raise

if __name__ == "__main__":
    tizgmusicproxy()
