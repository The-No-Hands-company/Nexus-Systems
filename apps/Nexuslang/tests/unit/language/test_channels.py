"""Tests for channel creation and send/receive semantics."""

import os
import sys

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../src")))

from nexuslang.errors import NxlRuntimeError
from tests.helpers.utils import NLPLTestBase


class TestChannels(NLPLTestBase):
    def test_channel_send_receive_round_trip(self):
        self.parse_and_execute(
            """
            set ch to create channel
            send 41 to ch
            set value to receive from ch
            """
        )

        assert self.get_variable("value") == 41

    def test_channel_send_receive_fifo(self):
        self.parse_and_execute(
            """
            set ch to create channel
            send 10 to ch
            send 20 to ch

            set first to receive from ch
            set second to receive from ch
            """
        )

        assert self.get_variable("first") == 10
        assert self.get_variable("second") == 20

    def test_channel_preserves_value_types(self):
        self.parse_and_execute(
            """
            set ch to create channel
            send "hello" to ch
            send [1, 2, 3] to ch

            set text_value to receive from ch
            set list_value to receive from ch
            """
        )

        assert self.get_variable("text_value") == "hello"
        assert self.get_variable("list_value") == [1, 2, 3]

    def test_receive_from_empty_channel_raises(self):
        with pytest.raises(NxlRuntimeError, match="empty channel"):
            self.parse_and_execute(
                """
                set ch to create channel
                set value to receive from ch
                """
            )

    def test_close_channel_then_receive_raises_closed_error(self):
        with pytest.raises(NxlRuntimeError, match="closed and empty channel"):
            self.parse_and_execute(
                """
                set ch to create channel
                close ch
                set value to receive from ch
                """
            )

    def test_send_to_closed_channel_raises(self):
        with pytest.raises(NxlRuntimeError, match="closed channel"):
            self.parse_and_execute(
                """
                set ch to create channel
                close ch
                send 1 to ch
                """
            )

    def test_channel_send_transfers_payload_snapshot(self):
        self.parse_and_execute(
            """
            set ch to create channel
            set payload to [1, 2, 3]
            send payload to ch
            set payload[0] to 99
            set received to receive from ch
            """
        )

        assert self.get_variable("received") == [1, 2, 3]
