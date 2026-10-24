"""Sparkplug B Protobuf bindings for the conformance harness.

This package exists so ``from tahu import sparkplug_b_pb2`` resolves to a real
package instead of relying on implicit namespace packages. That keeps the
import path identical inside the Docker image and in a local checkout, which
matters because the bindings are generated at image build time rather than
committed to the repository.

``sparkplug_b_pb2.py`` is generated from ``sparkplug_b.proto`` by
``grpc_tools.protoc`` and is therefore NOT tracked in git; see the
``.gitignore`` in this directory.
"""
