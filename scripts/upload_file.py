import sys
import os
import json
import cgi

def main():
	form = cgi.FieldStorage(
		fp=sys.stdin.buffer,
		environ=os.environ,
		keep_blank_values=True
	)

	if not form:
		return sys.stdout.write("Status: 404 File Not Found\r\n")

	file_field = form["file"]
	filename = file_field.filename
	file_content = file_field.file.read()

	with open(filename, "wb") as file:
		file.write(file_content)

	# returning http reponse
	sys.stdout.write("Status: 201 File Created\r\n\r\n<h1>success</h1>")

if __name__ == "__main__":
	main()