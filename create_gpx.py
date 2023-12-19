import geopy.distance
import gpxpy
import folium
import argparse
import os
import time
import webbrowser

class TrackPoint:

    km_mi_conversion = 0.621371
    m_ft_conversion = 3.28084
    data_seperator = ','
    key_seperator = '>'

    def __init__(self, lat, lon, sats, ele, date, time, crs, spd) -> None:
        self.lat = lat
        self.lon = lon
        self.sats = sats
        self.ele = self.__process_ele(ele)
        self.date = date
        self.time = self.__process_time(time)
        self.crs = crs
        self.spd = spd

    def __process_ele(self, e):
        return int(int(e) / self.m_ft_conversion)
    
    def __process_time(self, tm):
        return 'T' + tm + 'Z'
    
    def generate_trackpoint(self):
        trkpt = """<trkpt lat="{lat}" lon="{long}"><ele>{ele}</ele><time>{date}{time}</time></trkpt>"""
        return trkpt.format(
            lat=self.lat, 
            long=self.lon, 
            ele=self.ele, 
            date=self.date, 
            time=self.time
        )

    def waypoint_distance(self, end_lat, end_lon):
        start_point = (self.lat, self.lon)
        end_point = (end_lat, end_lon)
        return geopy.distance.distance(start_point, end_point).km
    
    @classmethod
    def process_raw_data(cls, input_file):
        with open(input_file, 'r') as f:
            raw_data = f.readlines()
        
        waypoint_list = []
        for data in raw_data:
            lines = data.strip().split(cls.data_seperator)
            trkpt_data = {}
            for line in lines:
                key, value = line.split(cls.key_seperator)
                trkpt_data[key] = value
            waypoint_list.append(trkpt_data)

        waypoint_data = []
        for point in waypoint_list:
            location = cls(
                point['lat'], 
                point['lon'], 
                point['sats'], 
                point['alti'], 
                point['date'], 
                point['time'], 
                point['course'], 
                point['speed']
            )
            waypoint_data.append(location)
            name = waypoint_data[-1].date + waypoint_data[-1].time
        return waypoint_data, name
    
    @classmethod
    def generate_gpx_file(cls, output_filename, trkpt_list, name):
        gpx_struct = cls.generate_gpx_structure(trkpt_list, name)
        with open(output_filename, "w") as f:
            f.write(gpx_struct)
        print(f"Success: Generated GPX file as {output_filename}")
        return gpx_struct

    @staticmethod
    def total_trip_distance(wypt_data):
        try:
            distance_list = []
            for i, wypt in enumerate(wypt_data):
                distance = wypt.waypoint_distance(
                    wypt_data[i+1].lat, 
                    wypt_data[i+1].lon
                )
                distance_list.append(distance)
        except IndexError:
            pass
        return sum(distance_list)
    
    @staticmethod
    def generate_trkpt_list(waypoint_data):
        trkpt_list =[]
        for waypoint in waypoint_data:
            trackpoint = waypoint.generate_trackpoint()
            trkpt_list.append(trackpoint)
        return trkpt_list
    
    @staticmethod
    def generate_gpx_structure(trkpt_list, name):
        trkpts =''
        for trkpt in trkpt_list:
            trkpts += trkpt
        with open('gpx_template') as f:
            gpx_struct = f.read().format(time=name, name=name, trackpoints=trkpts)
        return gpx_struct


def output_filename(filename):
    timestamp = time.time()
    formatted_time = time.strftime("%y%m%d_%H%M%S", time.localtime(timestamp))
    return f"{formatted_time}-{filename}"

def parse_gpx(file_path):
    with open(file_path, 'r') as gpx_file:
        return gpxpy.parse(gpx_file)

def extract_track_data(gpx_data):
    track_data = []
    for track in gpx_data.tracks:
        for segment in track.segments:
            for point in segment.points:
                track_data.append((point.latitude, point.longitude))
    return track_data

def create_map(track_data, out_file):
    avg_lat = sum(point[0] for point in track_data) / len(track_data)
    avg_lon = sum(point[1] for point in track_data) / len(track_data)
    map_center = [avg_lat, avg_lon]
    my_map = folium.Map(location=map_center, zoom_start=16)
    folium.PolyLine(locations=track_data, color='blue').add_to(my_map)
    my_map.save(f'{out_file}.html')
    map_path = f'file:///{os.getcwd()}/{out_file}.html'
    webbrowser.open_new_tab(map_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Arduino GPS Data to GPX')
    parser.add_argument('text', type=str, help='The Input File')
    parser.add_argument('-m', '--map', action='store_true', help='Generate Map')
    args = parser.parse_args()
    filename = args.text
    out_file = output_filename(filename)

    if filename:
        if os.path.isfile(f'{filename}.TXT'):
            print()
            print(f'Generate map: {args.map}')
            print()
            print(f'Using Input File: {filename}.TXT')
            try:
                waypoint_data, name = TrackPoint.process_raw_data(f'{filename}.TXT')
                trackpoint_list = TrackPoint.generate_trkpt_list(waypoint_data)
                TrackPoint.generate_gpx_file(f'{out_file}.gpx', trackpoint_list, name)
                distance_travelled = TrackPoint.total_trip_distance(waypoint_data)
                print(f"Total Trip Distance: {distance_travelled:.02f} kilometers")
                if args.map:
                    print()
                    gpx_data = parse_gpx(f'{out_file}.gpx')
                    track_data = extract_track_data(gpx_data)
                    create_map(track_data, out_file)
                    print(f'Map Generated at {out_file}.html')
            except ValueError:
                print()
                print('File is not compatible, check input file.')

        else:
            print()
            print(f'{filename}.TXT does not exist')
    
