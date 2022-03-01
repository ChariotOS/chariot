from bs4 import BeautifulSoup 
import requests
import argparse
import sys
import signal

DEFAULT_SERVER = "http://localhost"
DEFAULT_PORT   = 3000

def pretty_print(text):
    soup = BeautifulSoup(text, 'html.parser')
    return soup.h1.string


def sanitize(text, field):

    if not text:
        print(f"You must provide {field}")
        sys.exit()

    if not text.isalnum():
        print("Please only use alphanumeric characters")
        sys.exit()

    return text.lower()


def post_req(server, port, pid, uid, first, last, data):

    print("Attempting submission...")

    files = {'file': (data, open(data, 'rb'), 'application/gzip')}

    try: 
        r = requests.post(f"{server}:{port}/projects/{pid}/users/{uid}-{first}-{last}", files=files)
    except requests.exceptions.RequestException as e:
        print("Could not complete remote request. Is the server running?")
        sys.exit(1)

    if r.status_code != 200:
        print(f"Submission failed ({r.status_code}): {pretty_print(r.text)}")
        sys.exit(1)

    print(f"OK: {r.text}")


def graceful_exit(signal, frame):
    print("\nAborting submission.")
    sys.exit(0)


if __name__ == '__main__':

    signal.signal(signal.SIGINT, graceful_exit)

    parser = argparse.ArgumentParser(description='Client submission script for class projects')
    parser.add_argument('project', help='project name')
    parser.add_argument('file', help='the submission file')
    parser.add_argument('-s', '--server', action='store', default=DEFAULT_SERVER, help='submission server')
    parser.add_argument('-p', '--port', action='store', type=int, default=DEFAULT_PORT, help='submission server port')

    args = parser.parse_args()

    first = sanitize(input("Please enter your first name: "), 'first')
    last  = sanitize(input("Please enter your last name: "), 'last')
    uid   = sanitize(input("Please enter your student ID: "), 'uid')

    post_req(args.server, args.port, args.project, uid, first, last, args.file)
