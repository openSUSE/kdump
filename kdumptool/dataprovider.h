/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef DATAPROVIDER_H
#define DATAPROVIDER_H

#include <cstdio>
#include <cstdarg>

#include "global.h"

class Progress;

//{{{ DataProvider -------------------------------------------------------------

/**
 * This interface is part of the transfer mechanism: It provides the data
 * which the Transfer class can use.
 */
class DataProvider {

    public:

        /**
         * Destructor which must be provided so that it can be overwritten
         * by subclasses.
         */
        virtual ~DataProvider() {}

        /**
         * This method is called before the DataProvider::prepare() method is
         * called the first time. This can be used to actually open the
         * connection, start the process or something like that.
         *
         * @exception KError if any error happened
         */
        virtual void prepare()
        throw (KError) = 0;

        /**
         * This method gets called repeatedly
         *
         * @param[in] buffer the buffer where the data should be written to
         * @param[in] maxread the size of the buffer which means that maximum
         *            bytes that should be read
         * @return the number of bytes that actually have been read
         *
         * @exception KError when something goes wrong
         */
        virtual size_t getData(char *buffer, size_t maxread)
        throw (KError) = 0;

        /**
         * This method gets called after the last DataProvider::getData()
         * call. This can be used to do some cleanup, like closing the file
         * or terminating the process that provides the data.
         *
         * @exception KError if any error happened
         */
        virtual void finish()
        throw (KError) = 0;

        /**
         * Sets a progress notifier. The caller has to delete @p progress,
         * not this implementation.
         *
         * @param[in] progress the progress notifier.
         */
        virtual void setProgress(Progress *progress)
        throw () = 0;
};

//}}}
//{{{ AbstractDataProvider -----------------------------------------------------

/**
 * Abstract DataProvider that provides default implementations for
 * start(), stop() and the setProgress() function.
 */
class AbstractDataProvider : public DataProvider {

    public:

        /**
         * Creates a new AbstractDataProvider.
         */
        AbstractDataProvider()
        throw ();

        /**
         * Empty implementation (beside from starting the progress)
         * of DataProvider::prepare().
         */
        void prepare()
        throw (KError);

        /**
         * Empty implementation (beside from stopping the progress)
         * of DataProvider::finish().
         */
        void finish()
        throw (KError);

        /**
         * Sets the progress.
         *
         * @see DataProvider::setProgress()
         */
        void setProgress(Progress *progress)
        throw ();

        /**
         * Returns the progress.
         *
         * @return the Progress or @p NULL if not progress has been set
         */
        Progress *getProgress() const
        throw ();

    private:
        Progress *m_progress;
};

//}}}
//{{{ FileDataProvider ---------------------------------------------------------

/**
 * DataProvider that gets the data from file.
 */
class FileDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new FileDataProvider object.
         *
         * @param[in] filename the name of the file
         */
        FileDataProvider(const char *filename)
        throw ();

        /**
         * Actually opens the file.
         *
         * @see DataProvider::prepare()
         */
        void prepare()
        throw (KError);

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

        /**
         * Closes the file.
         *
         * @see DataProvider::finish()
         */
        virtual void finish()
        throw (KError);

    private:
        std::string m_filename;
        FILE *m_file;
        loff_t m_fileSize;
};

//}}}
//{{{ BufferDataProvider -------------------------------------------------------

/**
 * BufferDataProvider that gets the data from file.
 */
class BufferDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new BufferDataProvider object.
         *
         * @param[in] data the buffer
         */
        BufferDataProvider(const ByteVector &data)
        throw ();

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

    private:
        ByteVector m_data;
        unsigned long long m_currentPos;
};

//}}}
//{{{ ProcessDataProvider ------------------------------------------------------

/**
 * ProcessDataProvider is a DataProvider that gets the data from stdout from
 * a process. It does not make sense to set a Progress notifier for that type
 * of DataProvider because we don't know when the data stream ends.
 */
class ProcessDataProvider : public AbstractDataProvider {

    public:

        /**
         * Creates a new BufferDataProvider object.
         *
         * @param[in] data the buffer
         */
        ProcessDataProvider(const char *cmdline)
        throw ();

        /**
         * Starts the process
         *
         * @see DataProvider::prepare()
         */
        virtual void prepare()
        throw (KError);

        /**
         * Provides the data.
         *
         * @see DataProvider::getData()
         */
        size_t getData(char *buffer, size_t maxread)
        throw (KError);

        /**
         * Terminates the process.
         *
         * @see DataProvider::finish()
         */
        virtual void finish()
        throw (KError);

    private:
        std::string m_cmdline;
        FILE *m_processFile;
};

//}}}


#endif /* DATAPROVIDER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
