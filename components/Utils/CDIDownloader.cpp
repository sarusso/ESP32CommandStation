/**********************************************************************
ESP32 COMMAND STATION

COPYRIGHT (c) 2017-2021 Mike Dunston

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see http://www.gnu.org/licenses
**********************************************************************/

#include "CDIDownloader.hxx"
#include "StringUtils.hxx"
#include <utils/Base64.hxx>
#include "StringUtils.hxx"

CDIDownloadHandler::CDIDownloadHandler(Service *service, openlcb::Node *node,
                    openlcb::MemoryConfigHandler *memcfg)
                    : CallableFlow<CDIDownloadRequest>(service),
                    client_(node, memcfg)
{
}

StateFlowBase::Action CDIDownloadHandler::entry()
{
    LOG(INFO, "[CDI:%s] Requesting CDI XML",
        esp32cs::node_id_to_string(request()->target.id).c_str());
    string res = DOWNLOAD_START;
    res += "\n";
    request()->socket->send_text(res);
    tgt_ = esp32cs::node_id_to_string(request()->target.id);
    return call_immediately(STATE(download_chunk));
}

StateFlowBase::Action CDIDownloadHandler::download_chunk()
{
    LOG(INFO, "[CDI:%s] Requesting CDI XML (%u -> %u)",
        tgt_.c_str(), request()->offs,
        request()->offs + CHUNK_SIZE);
    return invoke_subflow_and_wait(&client_, STATE(chunk_complete),
        openlcb::MemoryConfigClientRequest::READ_PART, request()->target,
        openlcb::MemoryConfigDefs::SPACE_CDI, request()->offs, CHUNK_SIZE);
}

StateFlowBase::Action CDIDownloadHandler::chunk_complete()
{
    auto b = get_buffer_deleter(full_allocation_result(&client_));
    if (b->data()->resultCode)
    {
        LOG_ERROR("[CDI:%s] CDI XML download (%u->%u) returned code: %d",
                  tgt_.c_str(), request()->offs,
                  request()->offs + CHUNK_SIZE, b->data()->resultCode);
        request()->attempts++;
        if (request()->attempts <= MAX_ATTEMPTS)
        {
            LOG_ERROR("[CDI:%s] Failed to download CDI: %d (%d/%d)",
                      tgt_.c_str(), b->data()->resultCode,
                      request()->attempts, MAX_ATTEMPTS);
            return yield_and_call(STATE(download_chunk));
        }
        string res =
            StringPrintf(DOWNLOAD_FAILED_ERR, b->data()->resultCode);
        res += "\n";
        request()->socket->send_text(res);
    }
    else
    {
        LOG(INFO, "[CDI:%s] (%u->%u) Received", tgt_.c_str(), request()->offs,
            request()->offs + b->data()->payload.length());
        // Check if we have a null in the payload, this indicates end of stream
        // for the CDI data, RR-CirKits uses this rather than returning
        // out-of-bounds when reading beyond the payload size.
        bool eofFound = (b->data()->payload.find('\0') != std::string::npos);

        esp32cs::remove_nulls_and_FF(b->data()->payload, true);

        string encoded =
            StringPrintf(STREAM_SEGMENT_PART, request()->segment++,
                base64_encode(b->data()->payload).c_str());
        encoded += "\n";
        request()->socket->send_text(encoded);

        if (eofFound)
        {
            LOG(INFO, "[CDI:%s] CDI XML null byte detected", tgt_.c_str());
            // CDI data downloaded fully and streamed to the websocket,
            // send a message with basic snip data and flag to indicate
            // that the full CDI has been sent. This will trigger the
            // browser to parse the CDI and render the config dialog.
            string serialized =
                    StringPrintf(
                        R"!^!("node_id":%)!^!" PRIu64 R"!^!(,"has_snip":true,"has_cdi":true,)!^!"
                        R"!^!("is_train":false,"fdi":false)!^!",
                        request()->target.id);
            encoded =
                StringPrintf(DOWNLOAD_COMPLETE, request()->field.c_str(),
                    serialized.c_str());
            encoded += "\n";
            request()->socket->send_text(encoded);
        }
        else
        {
            // move to next chunk and start the download
            request()->offs += CHUNK_SIZE;
            return call_immediately(STATE(download_chunk));
        }
    }
    return exit();
}
