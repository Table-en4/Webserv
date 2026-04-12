import sys
import base64
import json
import os

JWT_SECRET_KEY = "cacaprout"

def decode_jwt_payload(token):
    try:
        segments = token.split('.')
        if len(segments) != 3:
            raise ValueError("Format de token invalide")
        payload_b64 = segments[1]
        missing_padding = len(payload_b64) % 4
        if missing_padding:
            payload_b64 += '=' * (4 - missing_padding)
        decoded_bytes = base64.urlsafe_b64decode(payload_b64)
        return json.loads(decoded_bytes.decode('utf-8'))
    except Exception as e:
        return None

def parse_cookies(cookie_header):
    cookies = {}
    for part in cookie_header.split(';'):
        part = part.strip()
        if '=' in part:
            key, value = part.split('=', 1)
            cookies[key.strip()] = value.strip()
    return cookies

def render_template(path, **kwargs):
    with open(path, "r", encoding="utf-8") as f:
        html = f.read()
    for key, value in kwargs.items():
        html = html.replace("{{" + key + "}}", value)
    return html

def main():
    # CORRECTION : HTTP_COOKIE et non COOKIE
    cookie_header = os.environ.get("HTTP_COOKIE", "")

    if not cookie_header:
        sys.stdout.write("Status: 403 Not authenticated\r\n\r\n<h1>Not auth</h1>")
        return

    # CORRECTION : parser les cookies correctement
    cookies = parse_cookies(cookie_header)
    jwt = cookies.get("token", None)

    if not jwt:
        sys.stdout.write("Status: 403 No JWT\r\n\r\n<h1>No token</h1>")
        return

    payload = decode_jwt_payload(jwt)
    if not payload:
        sys.stdout.write("Status: 403 Invalid JWT\r\n\r\n<h1>Invalid token</h1>")
        return

    first_name = payload.get("first_name", "")
    last_name = payload.get("last_name", "")
    username = payload.get("username", "")

    html = render_template(
		"profile.html",
		username=username,
		first_name=first_name,
		last_name=last_name
	)

    sys.stdout.write("Content-Type: text/html\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(html)

if __name__ == "__main__":
    main()
	