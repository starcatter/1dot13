// netshim: RakNet 3.401 API-compat header. The wrapper instantiates one and passes
// its address to FileListTransfer::Send; the shim streams from FileList memory instead.
#pragma once
class IncrementalReadInterface
{
public:
	virtual ~IncrementalReadInterface() {}
};
