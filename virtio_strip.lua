local virtio_strip_proto = Proto("virtiostrip", "Virtio Header Stripper")

function virtio_strip_proto.dissector(buffer, pinfo, tree)
    if buffer:len() < 10 then return end

    pinfo.cols.protocol = "VIRTIO_STRIP"

    local header_tree = tree:add(virtio_strip_proto, buffer(0,10), "Virtio Header")
    header_tree:add(buffer(0,1), "Flags: " .. buffer(0,1):uint())
    header_tree:add(buffer(1,1), "GSO Type: " .. buffer(1,1):uint())
    header_tree:add(buffer(2,2), "Header Length: " .. buffer(2,2):le_uint())
    header_tree:add(buffer(4,2), "GSO Size: " .. buffer(4,2):le_uint())
    header_tree:add(buffer(6,2), "Checksum Start: " .. buffer(6,2):le_uint())
    header_tree:add(buffer(8,2), "Checksum Offset: " .. buffer(8,2):le_uint())

    local newbuf = buffer(10):tvb()
    local eth_dissector = Dissector.get("eth_withoutfcs")
    if eth_dissector then
        eth_dissector:call(newbuf, pinfo, tree)
    end
end

local eth_table = DissectorTable.get("wtap_encap")
eth_table:add(1, virtio_strip_proto)