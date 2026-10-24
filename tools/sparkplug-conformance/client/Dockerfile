# Sparkplug B conformance harness — Host Application simulator.
#
# The Tahu schema is compiled into Python bindings at BUILD time rather than
# vendored as generated code, so the decoder can never drift away from the
# checked-in .proto file.
FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY tahu/ ./tahu/
COPY conformance.py ./

# Compile the official Eclipse Tahu schema (proto2, package
# org.eclipse.tahu.protobuf) into Python bindings.
RUN python -m grpc_tools.protoc \
        --proto_path=./tahu \
        --python_out=./tahu \
        ./tahu/sparkplug_b.proto \
 && test -s ./tahu/sparkplug_b_pb2.py

CMD ["python", "conformance.py"]
