from abc import ABC, abstractmethod
from pathlib import Path
import tarfile
import io
import stat

from ert.data import RecordTransmitter, BlobRecord

_BIN_FOLDER = "bin"


class RecordTransformation(ABC):
    @abstractmethod
    async def transform(
        self, transmitter: RecordTransmitter, mime: str, runpath: Path, location: Path
    ) -> None:
        raise NotImplementedError("not implemented")


class FileRecordTransformation(RecordTransformation):
    async def transform(
        self,
        transmitter: RecordTransmitter,
        mime: str,
        runpath: Path,
        location: Path,
    ) -> None:
        _location = runpath / location
        await transmitter.dump(_location, mime)


class TarRecordTransformation(RecordTransformation):
    async def transform(
        self, transmitter: RecordTransmitter, mime: str, runpath: Path, location: Path
    ) -> None:
        record = await transmitter.load()
        if isinstance(record, BlobRecord):
            with tarfile.open(fileobj=io.BytesIO(record.data), mode="r") as tar:
                tar.extractall(runpath / location)
        else:
            raise TypeError("Record needs to be a BlobRecord type!")


class ExecutableRecordTransformation(RecordTransformation):
    async def transform(
        self, transmitter: RecordTransmitter, mime: str, runpath: Path, location: Path
    ) -> None:
        # pre-make bin folder if neccessary
        Path(runpath / _BIN_FOLDER).mkdir(parents=True, exist_ok=True)

        # create file(s)
        await transmitter.dump(runpath / _BIN_FOLDER / location, mime)

        # post-proccess if neccessary
        path = runpath / _BIN_FOLDER / location
        st = path.stat()
        path.chmod(st.st_mode | stat.S_IEXEC)